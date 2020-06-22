#ifndef UpdateQtCommand__H
#define UpdateQtCommand__H

#include <itkCommand.h>
#include <itkObject.h>
#include <itkEventObject.h>

class UpdateQtCommand : public itk::Command
{
public:
	typedef UpdateQtCommand Self;
	typedef itk::Command Superclass;
	typedef itk::SmartPointer<Self> Pointer;
	itkNewMacro(Self);
	void Execute(itk::Object*, const itk::EventObject&);
	void Execute(const itk::Object*, const itk::EventObject&);
};

#endif
