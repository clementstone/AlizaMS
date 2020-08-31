/*=========================================================================

  Program: GDCM (Grassroots DICOM). A DICOM library

  Copyright (c) 2006-2011 Mathieu Malaterre
  All rights reserved.
  See Copyright.txt or http://gdcm.sourceforge.net/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "mdcmScanner.h"
#include "mdcmReader.h"
#include "mdcmGlobal.h"
#include "mdcmDicts.h"
#include "mdcmDict.h"
#include "mdcmDictEntry.h"
#include "mdcmStringFilter.h"
#include "mdcmProgressEvent.h"
#include "mdcmFileNameEvent.h"
#include <algorithm>

namespace mdcm
{

Scanner::~Scanner() {}

void Scanner::ClearTags()
{
  Tags.clear();
}

void Scanner::ClearSkipTags()
{
  SkipTags.clear();
}

void Scanner::AddSkipTag(Tag const & t)
{
  SkipTags.insert(t);
  assert(0); // not implemented
}

// Warning: API is passing a public tag (no way to specify a private tag)
void Scanner::AddPrivateTag(PrivateTag const & t)
{
  static const Global &g = GlobalInstance;
  static const Dicts &dicts = g.GetDicts();
  const DictEntry &entry = dicts.GetDictEntry(t);
  if(entry.GetVR() & VR::VRASCII)
  {
    PrivateTags.insert(t);
  }
  else if(entry.GetVR() == VR::INVALID)
  {
    mdcmWarningMacro("Only tag with known VR are allowed. Tag " << t << " will be discarded");
  }
  else
  {
    assert(entry.GetVR() & VR::VRBINARY);
    PrivateTags.insert(t);
  }
}

void Scanner::AddTag(Tag const & t)
{
  static const Global &g = GlobalInstance;
  static const Dicts &dicts = g.GetDicts();
  const DictEntry &entry = dicts.GetDictEntry(t);
  if(entry.GetVR() & VR::VRASCII)
  {
    Tags.insert(t);
  }
  else if(entry.GetVR() == VR::INVALID)
  {
    mdcmWarningMacro("Only tag with known VR are allowed. Tag " << t << " will be discarded");
  }
  else
  {
    assert(entry.GetVR() & VR::VRBINARY);
    Tags.insert(t);
  }
}

bool Scanner::Scan(Directory::FilenamesType const & filenames)
{
  this->InvokeEvent(StartEvent());
  if(!Tags.empty() || !PrivateTags.empty())
  {
    Mappings.clear();
    Mappings[""]; // Create a fake table for dummy file
    Filenames = filenames;
    // Find the tag with the highest value (get the one from the end of the std::set)
    Tag last;
    if(!Tags.empty())
    {
      TagsType::const_reverse_iterator it1 = Tags.rbegin();
      const Tag & publiclast = *it1;
      last = publiclast;
    }
    if(!PrivateTags.empty())
    {
      PrivateTagsType::const_reverse_iterator pit1 = PrivateTags.rbegin();
      Tag privatelast = *pit1;
      if(last < privatelast) last = privatelast;
    }

    StringFilter sf;
    Directory::FilenamesType::const_iterator it = Filenames.begin();
    const double progresstick = 1. / (double)Filenames.size();
    Progress = 0;
    for(; it != Filenames.end(); ++it)
    {
      Reader reader;
      const char *filename = it->c_str();
      assert(filename);
      reader.SetFileName(filename);
      bool read = false;
      try
      {
        read = reader.ReadUpToTag(last, SkipTags);
      }
      catch(...)
      {
        mdcmWarningMacro("Failed to read:" << filename);
      }
      if(read)
      {
        sf.SetFile(reader.GetFile());
        Scanner::ProcessPublicTag(sf, filename);
      }
      Progress += progresstick;
      ProgressEvent pe;
      pe.SetProgress(Progress);
      this->InvokeEvent(pe);
      FileNameEvent fe(filename);
      this->InvokeEvent(fe);
    }
  }
  this->InvokeEvent(EndEvent());
  return true;
}

void Scanner::Print(std::ostream & os) const
{
  os << "Values:\n";
  for(ValuesType::const_iterator it = Values.begin() ; it != Values.end();
    ++it)
  {
    os << *it << "\n";
  }
  os << "Mapping:\n";
  Directory::FilenamesType::const_iterator file = Filenames.begin();
  for(; file != Filenames.end(); ++file)
  {
    const char *filename = file->c_str();
    assert(filename && *filename);
    bool b = IsKey(filename);
    const char *comment = !b ? "could not be read" : "could be read";
    os << "Filename: " << filename << " (" << comment << ")\n";
    if(Mappings.find(filename) != Mappings.end())
    {
      const TagToValue &mapping = GetMapping(filename);
      TagToValue::const_iterator it = mapping.begin();
      for(; it != mapping.end(); ++it)
      {
        const Tag & tag = it->first;
        const char *value = it->second;
        os << tag << " -> [" << value << "]\n";
      }
    }
  }
}

Scanner::TagToValue const & Scanner::GetMapping(const char * filename) const
{
  assert(filename && *filename);
  if(Mappings.find(filename) != Mappings.end())
    return Mappings.find(filename)->second;
  return Mappings.find("")->second; // dummy file could not be found
}

bool Scanner::IsKey(const char * filename) const
{
  assert(filename && *filename);
  MappingType::const_iterator it2 = Mappings.find(filename);
  return it2 != Mappings.end();
}

Directory::FilenamesType Scanner::GetKeys() const
{
  Directory::FilenamesType keys;

  Directory::FilenamesType::const_iterator file = Filenames.begin();
  for(; file != Filenames.end(); ++file)
  {
    const char *filename = file->c_str();
    if(IsKey(filename))
    {
      keys.push_back(filename);
    }
  }
  assert(keys.size() <= Filenames.size());
  return keys;
}

const char* Scanner::GetValue(const char * filename, Tag const & t) const
{
  assert(Tags.find(t) != Tags.end());
  TagToValue const &ftv = GetMapping(filename);
  if(ftv.find(t) != ftv.end())
  {
    return ftv.find(t)->second;
  }
  return NULL;
}

const char *Scanner::GetFilenameFromTagToValue(Tag const & t, const char * valueref) const
{
  const char *filenameref = 0;
  if(valueref)
  {
    Directory::FilenamesType::const_iterator file = Filenames.begin();
    size_t len = strlen(valueref);
    if(len && valueref[len - 1] == ' ')
    {
      --len;
    }
    for(; file != Filenames.end() && !filenameref; ++file)
    {
      const char *filename = file->c_str();
      const char * value = GetValue(filename, t);
      if(value && strncmp(value, valueref, len) == 0)
      {
        filenameref = filename;
      }
    }
  }
  return filenameref;
}

Directory::FilenamesType
Scanner::GetAllFilenamesFromTagToValue(Tag const &t, const char *valueref) const
{
  Directory::FilenamesType theReturn;
  if(valueref)
  {
    const std::string valueref_str = String<>::Trim(valueref);
    Directory::FilenamesType::const_iterator file = Filenames.begin();
    for(; file != Filenames.end(); ++file)
    {
      const char *filename = file->c_str();
      const char * value = GetValue(filename, t);
      const std::string value_str = String<>::Trim(value);
      if(value_str == valueref_str)
      {
        theReturn.push_back(filename);
      }
    }
  }
  return theReturn;
}

Scanner::TagToValue const & Scanner::GetMappingFromTagToValue(Tag const & t, const char * valueref) const
{
  return GetMapping(GetFilenameFromTagToValue(t, valueref));
}

Scanner::ValuesType Scanner::GetValues(Tag const &t) const
{
  ValuesType vt;
  Directory::FilenamesType::const_iterator file = Filenames.begin();
  for(; file != Filenames.end(); ++file)
  {
    const char *filename = file->c_str();
    TagToValue const &ttv = GetMapping(filename);
    if(ttv.find(t) != ttv.end())
    {
      vt.insert(ttv.find(t)->second);
    }
  }
  return vt;
}

Directory::FilenamesType Scanner::GetOrderedValues(Tag const & t) const
{
  Directory::FilenamesType theReturn;
  Directory::FilenamesType::const_iterator file = Filenames.begin();
  for(; file != Filenames.end(); ++file)
  {
    const char * filename = file->c_str();
    TagToValue const &ttv = GetMapping(filename);
    if(ttv.find(t) != ttv.end())
    {
      std::string theVal = std::string(ttv.find(t)->second);
      if (std::find(theReturn.begin(), theReturn.end(), theVal) == theReturn.end())
      {
        theReturn.push_back(theVal); // only add new tags to the list
      }
    }
  }
  return theReturn;
}

void Scanner::ProcessPublicTag(StringFilter & sf, const char * filename)
{
  assert(filename);
  TagToValue &mapping = Mappings[filename];
  const File& file = sf.GetFile();

  const FileMetaInformation & header = file.GetHeader();
  const DataSet & ds = file.GetDataSet();
  TagsType::const_iterator tag = Tags.begin();
  for(; tag != Tags.end(); ++tag)
  {
    if(tag->GetGroup() == 0x2)
    {
      if(header.FindDataElement(*tag))
      {
        DataElement const & de = header.GetDataElement(*tag);
        std::string s = sf.ToString(de.GetTag());
        Values.insert(s);
        assert(Values.find(s) != Values.end());
        const char *value = Values.find(s)->c_str();
        assert(value);
        mapping.insert(TagToValue::value_type(*tag, value));
      }
    }
    else
    {
      if(ds.FindDataElement(*tag))
      {
        DataElement const & de = ds.GetDataElement(*tag);
        std::string s = sf.ToString(de.GetTag());
        Values.insert(s);
        assert(Values.find(s) != Values.end());
        const char *value = Values.find(s)->c_str();
        assert(value);
        mapping.insert(TagToValue::value_type(*tag, value));
      }
    }
  }
}

} // end namespace mdcm
