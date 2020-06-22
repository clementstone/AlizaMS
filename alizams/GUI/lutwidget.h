#ifndef LUTWIDGET_H
#define LUTWIDGET_H

#include <QWidget>
#include <QComboBox>

class LUTWidget: public QWidget
{
	Q_OBJECT
public:
	LUTWidget(QWidget(*)=NULL, Qt::WindowFlags=0);
	~LUTWidget();
	void add_items1();
	void add_items2();
	int get_lut() const;
	QComboBox * comboBox;
public slots:
	void set_lut(int);
};

#endif // LUTWIDGET_H
