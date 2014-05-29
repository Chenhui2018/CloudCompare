//##########################################################################
//#                                                                        #
//#                            CLOUDCOMPARE                                #
//#                                                                        #
//#  This program is free software; you can redistribute it and/or modify  #
//#  it under the terms of the GNU General Public License as published by  #
//#  the Free Software Foundation; version 2 of the License.               #
//#                                                                        #
//#  This program is distributed in the hope that it will be useful,       #
//#  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
//#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
//#  GNU General Public License for more details.                          #
//#                                                                        #
//#          COPYRIGHT: EDF R&D / TELECOM ParisTech (ENST-TSI)             #
//#                                                                        #
//##########################################################################

#include "ccHistogramWindow.h"
#include "ccGuiParameters.h"

//qCC_db
#include <ccColorScalesManager.h>

//Qt
#include <QCloseEvent>

//System
#include <assert.h>

ccHistogramWindow::ccHistogramWindow(QWidget* parent/*=0*/)
	: QGLWidget(parent)
	, m_viewInitialized(false)
	, m_numberOfClassesCanBeChanged(false)
	, m_associatedSF(0)
	, m_minVal(0)
	, m_maxVal(0)
	, m_maxHistoVal(0)
	, m_maxCurveValue(0)
	, m_xMinusButton(0)
	, m_yMinusButton(0)
	, m_xPlusButton(0)
	, m_yPlusButton(0)
	, m_buttonSize(13)
	, m_drawVerticalIndicator(false)
	, m_verticalIndicatorPositionPercent(0)
{
	memset(m_roi,0,sizeof(int)*4);

	setWindowTitle("Histogram");
	setFocusPolicy(Qt::StrongFocus);

	setMinimumSize(400,300);
	resize(400,375);

	//default font for text rendering
	m_renderingFont.setFamily(QString::fromUtf8("Arial"));
	m_renderingFont.setBold(false);
	//m_renderingFont.setWeight(75);
}

ccHistogramWindow::~ccHistogramWindow()
{
	clearInternal();
}

void ccHistogramWindow::clear()
{
	clearInternal();
	updateGL();
}

void ccHistogramWindow::clearInternal()
{
	m_histoValues.clear();
	m_maxHistoVal = 0;

	m_curveValues.clear();
	m_maxCurveValue = 0.0;
}

void ccHistogramWindow::closeEvent(QCloseEvent *event)
{
	clearInternal();
	event->accept();
}

void ccHistogramWindow::setInfoStr(const QString& str)
{
	m_infoStr = str;
}

void ccHistogramWindow::fromSF(ccScalarField* sf,
								unsigned initialNumberOfClasses/*=0*/,
								bool numberOfClassesCanBeChanged/*=true*/)
{
	assert(sf);

	m_associatedSF = sf;
	m_minVal = m_associatedSF->getMin();
	m_maxVal = m_associatedSF->getMax();
	m_numberOfClassesCanBeChanged = numberOfClassesCanBeChanged;

	setNumberOfClasses(initialNumberOfClasses);
};

void ccHistogramWindow::fromBinArray(	const std::vector<unsigned>& histoValues,
										double minVal,
										double maxVal)
{
	try
	{
		m_histoValues = histoValues;
	}
	catch(std::bad_alloc)
	{
		ccLog::Warning("[ccHistogramWindow::fromBinArray] Not enough memory!");
		return;
	}
	m_minVal = minVal;
	m_maxVal = maxVal;
	m_numberOfClassesCanBeChanged = false;

	//update max histogram value
	m_maxHistoVal = getMaxHistoVal();
}

void ccHistogramWindow::setCurveValues(const std::vector<double>& curveValues)
{
	try
	{
		m_curveValues = curveValues;
	}
	catch(std::bad_alloc)
	{
		ccLog::Warning("[ccHistogramWindow::fromBinArray] Not enough memory!");
	}
	
	//compute max curve value by the way
	m_maxCurveValue = 0.0;
	for (size_t i=0; i<m_curveValues.size(); ++i)
		m_maxCurveValue = std::max(m_maxCurveValue,m_curveValues[i]);
}

bool ccHistogramWindow::computeBinArrayFromSF(size_t binCount)
{
	//clear any existing histogram
	m_histoValues.clear();

	if (!m_associatedSF)
	{
		assert(false);
		ccLog::Error("[ccHistogramWindow::computeBinArrayFromSF] Need an associated SF!");
		return false;
	}

	if (binCount == 0)
	{
		assert(false);
		ccLog::Error("[ccHistogramWindow::computeBinArrayFromSF] Invalid number of classes!");
		return false;
	}

	//(try to) create new array
	try
	{
		m_histoValues.resize(binCount,0);
	}
	catch(std::bad_alloc)
	{
		ccLog::Warning("[ccHistogramWindow::computeBinArrayFromSF] Not enough memory!");
		return false;
	}

	double range = m_maxVal - m_minVal;
	if (range > 0.0)
	{
		unsigned count = m_associatedSF->currentSize();
		for (unsigned i=0; i<count; ++i)
		{
			double val = static_cast<double>(m_associatedSF->getValue(i));

			//we ignore values outside of [m_minVal,m_maxVal]
			if (val >= m_minVal && val <= m_maxVal)
			{
				size_t bin = static_cast<size_t>( floor((val-m_minVal)*static_cast<double>(binCount)/range) );
				++m_histoValues[std::min(bin,binCount-1)];
			}
		}
	}
	else
	{
		m_histoValues[0] = m_associatedSF->currentSize();
	}

	return true;
}

unsigned ccHistogramWindow::getMaxHistoVal()
{
	unsigned m_maxHistoVal = 0;
	
	for (size_t i=0; i<m_histoValues.size(); ++i)
		m_maxHistoVal = std::max(m_maxHistoVal,m_histoValues[i]);

	return m_maxHistoVal;
}

void ccHistogramWindow::setNumberOfClasses(size_t n)
{
	if (n == 0)
	{
		//invalid parameter
		assert(false);
		return;
	}

	if (n == m_histoValues.size())
	{
		//nothing to do
		return;
	}

	if (!m_numberOfClassesCanBeChanged)
	{
		assert(false);
		return;
	}

	if (m_associatedSF)
	{
		//dynamically recompute histogram values
		computeBinArrayFromSF(n);
	}

	//update max histogram value
	m_maxHistoVal = getMaxHistoVal();
}

void drawButton(int xButton, int yButton, int buttonSize)
{
	glBegin(GL_LINE_LOOP);
	glVertex2i(xButton,yButton);
	glVertex2i(xButton+buttonSize-1,yButton);
	glVertex2i(xButton+buttonSize-1,yButton-buttonSize+1);
	glVertex2i(xButton,yButton-buttonSize+1);
	glEnd();
}

//structure for recursive display of labels
struct hlabel
{
	int leftXpos; 			/**< left label center pos **/
	int leftXmax; 			/**< left label 'ROI' max **/
	double leftVal; 		/**< left label value **/

	int rightXpos;			/**< right label center pos **/
	int rightXmin;			/**< right label 'ROI' min **/
	double rightVal;		/**< right label value **/
};

void ccHistogramWindow::paintGL()
{
	makeCurrent();

	//for proper display of labels!
	m_renderingFont.setPointSize(ccGui::Parameters().defaultFontSize);
	QFontMetrics strMetrics(m_renderingFont);
	//precision (same as color scale)
	int precision = static_cast<int>(ccGui::Parameters().displayedNumPrecision);

	//background color
	const unsigned char* bkgCol = m_displayParameters.getBackgroundColor();
	glClearColor(	static_cast<float>(bkgCol[0])/255,
					static_cast<float>(bkgCol[1])/255,
					static_cast<float>(bkgCol[2])/255,
					1.0f );
	glClear(GL_COLOR_BUFFER_BIT);

	int w = width();
	int h = height();

	//we always reinit the OpenGL context (simpler, safer)
	glViewport(0,0,width(),height());
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0,w-1,0,h-1,-1.0,1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	//default color: text color
	const unsigned char* textCol = m_displayParameters.getDefaultColor();
	//Some versions of Qt seem to need glColorf instead of glColorub! (see https://bugreports.qt-project.org/browse/QTBUG-6217)
	glColor3f(	static_cast<float>(textCol[0])/255,
				static_cast<float>(textCol[1])/255,
				static_cast<float>(textCol[2])/255 );

	//margins, etc.
	const int c_outerMargin = 10;
	const int c_innerMargin = 2;
	const int c_ticksSize = 5;
	const int c_strHeight = strMetrics.height();
	const int c_strDescent = strMetrics.descent();

	//+/- buttons (top-right)
	if (m_numberOfClassesCanBeChanged)
	{
		int yMin = h-c_outerMargin;
		int yHalf = yMin-(m_buttonSize/2); //to cope with odd button sizes
		//"minus"
		m_xMinusButton = w-(c_outerMargin+2*m_buttonSize+c_innerMargin*2);
		m_yMinusButton = yMin;
		drawButton(m_xMinusButton,m_yMinusButton,m_buttonSize);
		glBegin(GL_LINES);
		glVertex2i(m_xMinusButton+c_innerMargin,yHalf);
		glVertex2i(m_xMinusButton+m_buttonSize-1-c_innerMargin-1,yHalf);
		glEnd();

		//"plus"
		m_xPlusButton = m_xMinusButton+m_buttonSize+2*c_innerMargin;
		m_yPlusButton = yMin;
		drawButton(m_xPlusButton,m_yPlusButton,m_buttonSize);
		glBegin(GL_LINES);
		glVertex2i(m_xPlusButton+c_innerMargin,yHalf);
		glVertex2i(m_xPlusButton+m_buttonSize-1-c_innerMargin-1,yHalf);
		glVertex2i(m_xPlusButton+m_buttonSize/2,yMin-m_buttonSize+c_innerMargin+1);
		glVertex2i(m_xPlusButton+m_buttonSize/2,yMin-c_innerMargin-1);
		glEnd();
	}
	else
	{
		m_xMinusButton = w;
		m_yMinusButton = h;
	}

	//top-left corner
	renderText(	c_outerMargin,
				c_outerMargin+c_strHeight-c_strDescent,
				strMetrics.elidedText(QString("%0 [%1 classes]").arg(m_infoStr).arg(m_histoValues.size()),
				Qt::ElideRight,
				m_xMinusButton-c_outerMargin),
				m_renderingFont );

	//can't go any further without data!
	if (m_histoValues.empty())
		return;

	//update horizontal & vertical axes position so that their labels can be properly displayed
	int maxYLabelWidth = strMetrics.width(QString::number(m_maxHistoVal));
	QString firstlabel = QString::number(m_minVal,'f',precision);
	int firstXLabelWidth = strMetrics.width(firstlabel);
	QString lastlabel = QString::number(m_maxVal,'f',precision);
	int lastXLabelWidth = strMetrics.width(lastlabel);
	m_roi[0] = c_outerMargin+std::max(maxYLabelWidth+c_innerMargin,firstXLabelWidth/2);	//Xmin
	m_roi[1] = c_outerMargin+c_strHeight+c_innerMargin+c_ticksSize;						//Ymin
	m_roi[2] = w-c_outerMargin-lastXLabelWidth/2;										//Xmax
	m_roi[3] = h-(2*c_outerMargin+c_strHeight+c_strHeight/2);							//Ymax

	//histogram width
	int dx = m_roi[2]-m_roi[0];
	//histogram height
	int dy = m_roi[3]-m_roi[1];
	if (dx<2 || dy<2)
		return;

	//draw both axes
	glBegin(GL_LINES);
	//vertical
	glVertex2i(m_roi[0],m_roi[1]);
	glVertex2i(m_roi[0],m_roi[3]);
	//horizontal
	glVertex2i(m_roi[0],m_roi[1]);
	glVertex2i(m_roi[2],m_roi[1]);
	glEnd();

	//horizontal labels
	{
		//draw first value tick & label
		glBegin(GL_LINES);
		glVertex2i(m_roi[0],m_roi[1]-c_ticksSize);
		glVertex2i(m_roi[0],m_roi[1]);
		glEnd();
		renderText(m_roi[0]-firstXLabelWidth/2, h-c_outerMargin, firstlabel, m_renderingFont);

		//draw last value tick & label
		glBegin(GL_LINES);
		glVertex2i(m_roi[0]+dx,m_roi[1]-c_ticksSize);
		glVertex2i(m_roi[0]+dx,m_roi[1]);
		glEnd();
		renderText(m_roi[0]+dx-lastXLabelWidth/2, h-c_outerMargin, lastlabel, m_renderingFont);

		//we recursively display the remaining horizontal labels (unitl there's no room left)
		hlabel centerLabel;
		centerLabel.leftXmax = firstXLabelWidth/2;
		centerLabel.rightXmin = dx-lastXLabelWidth/2;

		//free space available between first and last label?
		const int c_labelMargin = 2*(strMetrics.width("123456789")/9); //twice the mean (digit) character width
		if (centerLabel.leftXmax+2*c_labelMargin<centerLabel.rightXmin)
		{
			centerLabel.leftVal = m_minVal;
			centerLabel.leftXpos = 0;
			centerLabel.rightVal = m_maxVal;
			centerLabel.rightXpos = dx;
			std::vector<hlabel> currentLevellabels;
			currentLevellabels.push_back(centerLabel);

			bool proceedWithNextLevel=true;
			while (proceedWithNextLevel)
			{
				std::vector<hlabel> nextLevelLabels;
				std::vector< std::pair<int,QString> > strToDisplay;
				while (!currentLevellabels.empty())
				{
					hlabel currentLabel = currentLevellabels.back();
					currentLevellabels.pop_back();

					//draw corresponding tick
					int x = (currentLabel.leftXpos+currentLabel.rightXpos)/2;
					glBegin(GL_LINES);
					glVertex2i(m_roi[0]+x,m_roi[1]-c_ticksSize);
					glVertex2i(m_roi[0]+x,m_roi[1]);
					glEnd();

					//try to draw label as well
					double value = (currentLabel.leftVal+currentLabel.rightVal)/2.0;
					QString valueStr = QString::number(value,'f',precision);
					int valueStrHalfWidth = strMetrics.width(valueStr)/2;
					if (x-valueStrHalfWidth-c_labelMargin > currentLabel.leftXmax
						&& x+valueStrHalfWidth+c_labelMargin < currentLabel.rightXmin)
					{
						//we push the label str (and position) in 'strToDisplay' vector
						strToDisplay.push_back(std::pair<int,QString>(m_roi[0]+x-valueStrHalfWidth,valueStr));
						//renderText(m_roi[0]+x-valueStrHalfWidth, h-c_outerMargin, valueStr, m_renderingFont);
						//add sub labels to next level queue
						hlabel leftLabel = currentLabel;
						leftLabel.rightVal = value;
						leftLabel.rightXpos = x;
						leftLabel.rightXmin = x-valueStrHalfWidth;

						hlabel rightLabel = currentLabel;
						rightLabel.leftVal = value;
						rightLabel.leftXpos = x;
						rightLabel.leftXmax = x+valueStrHalfWidth;

						nextLevelLabels.push_back(leftLabel);
						nextLevelLabels.push_back(rightLabel);
					}
					else
					{
						//not enough space for current label! we stop here
						proceedWithNextLevel=false;
					}
				}

				if (proceedWithNextLevel)
				{
					//we only display labels if 'proceedWithNextLevel' is true
					while (!strToDisplay.empty())
					{
						renderText(strToDisplay.back().first, h-c_outerMargin, strToDisplay.back().second, m_renderingFont);
						strToDisplay.pop_back();
					}

					if (nextLevelLabels.empty())
						proceedWithNextLevel = false;
					else
						currentLevellabels = nextLevelLabels;
				}
			}
		}
	}

	//vertical labels
	{
		const int n = 4;
		for (int i=0; i<=n; ++i)
		{
			int y = m_roi[1] + (dy*i)/n;
			glBegin(GL_LINES);
			glVertex2i(m_roi[0]-5,y);
			glVertex2i(m_roi[0],y);
			glEnd();

			int vi = (int)((float)(m_maxHistoVal*i)/(float)n);
			QString valueStr = QString::number(vi);
			renderText(m_roi[0]-strMetrics.width(valueStr)-(c_ticksSize+c_innerMargin), h-(y-c_strHeight/2+c_strDescent), valueStr, m_renderingFont);
		}
	}

	//vertical scaling
	unsigned maxDisplayedHistoVal = std::max<unsigned>(m_maxHistoVal,1);
	double yScale = (double)dy/std::max<double>(m_maxCurveValue,maxDisplayedHistoVal);

	//the histogram itself
	unsigned totalSum = 0;
	unsigned partialSum = 0;
	{
		float x = static_cast<float>(m_roi[0]+1);
		float y = static_cast<float>(m_roi[1]+1);
		float barWidth = static_cast<float>(dx)/static_cast<float>(m_histoValues.size());

		bool useGradientColorForBars = m_displayParameters.useGradientColorForBars();
		ccColorScale::Shared colorScale = (m_associatedSF && m_associatedSF->getColorScale() ? m_associatedSF->getColorScale() : ccColorScalesManager::GetDefaultScale());
		assert(colorScale);
		for (size_t i=0; i<m_histoValues.size(); ++i)
		{
			totalSum += m_histoValues[i];
			if (static_cast<double>(i)/m_histoValues.size() < m_verticalIndicatorPositionPercent)
				partialSum += m_histoValues[i];

			//we take the 'normalized' value at the middle of the class
			double normVal = (static_cast<double>(i)+0.5) / m_histoValues.size();

			if (m_associatedSF)
			{
				//Equivalent SF value
				double scalarVal = m_minVal + (m_maxVal-m_minVal)*normVal;
				const colorType* col = m_associatedSF->getColor(static_cast<ScalarType>(scalarVal));
				glColor3ubv(col ? col : ccColor::lightGrey);
			}
			else if (useGradientColorForBars)
			{
				glColor3ubv(colorScale->getColorByRelativePos(normVal));
			}

			float barHeight = static_cast<float>(m_histoValues[i] * yScale);
			glBegin(GL_QUADS);
			glVertex2f(x,y);
			glVertex2f(x+barWidth,y);
			glVertex2f(x+barWidth,y+barHeight);
			glVertex2f(x,y+barHeight);
			glEnd();

			x+=barWidth;
		}
	}

	//overlay curve?
	if (m_curveValues.size() > 1)
	{
		float x = static_cast<float>(m_roi[0]+1);
		float y = static_cast<float>(m_roi[1]+1);
		float step = static_cast<float>(dx)/static_cast<float>(m_curveValues.size());

		//same as text color by default
		glColor3ubv(textCol);

		glBegin(GL_LINE_STRIP);
		for (size_t i=0; i<m_curveValues.size(); ++i)
		{
			glVertex2f(x,y+static_cast<float>(m_curveValues[i]*yScale));
			x+=step;
		}
		glEnd();
	}

	//vertical hint
	if (m_drawVerticalIndicator)
	{
		//red by default
		glColor3ubv(ccColor::red);

		//horizontal position
		int x = m_roi[0] + 1 + (int)(m_verticalIndicatorPositionPercent*(double)dx);
		int y = m_roi[3] - c_strHeight;

		glBegin(GL_LINES);
		glVertex2i(x,m_roi[1]);
		glVertex2i(x,m_roi[3]);
		glEnd();

		bool leftSide = (m_verticalIndicatorPositionPercent > 0.5);
		unsigned bin = static_cast<unsigned>(m_verticalIndicatorPositionPercent * m_histoValues.size());
		QString valueStr = QString("bin %0").arg(bin);

		//Some versions of Qt seem to need glColorf instead of glColorub! (see https://bugreports.qt-project.org/browse/QTBUG-6217)
		glColor3f(	static_cast<float>(textCol[0])/255,
					static_cast<float>(textCol[1])/255,
					static_cast<float>(textCol[2])/255 );
		renderText(leftSide ? x-strMetrics.width(valueStr)-c_innerMargin : x+c_innerMargin, h-y, valueStr, m_renderingFont);
		y -= (c_strHeight+c_innerMargin);

		valueStr = QString("< %0 %").arg(100.0*static_cast<double>(partialSum)/static_cast<double>(totalSum),0,'f',3);
		renderText(leftSide ? x-strMetrics.width(valueStr)-c_innerMargin : x+c_innerMargin, h-y, valueStr, m_renderingFont);
		y -= (c_strHeight+c_innerMargin);

		valueStr = QString("val = %0").arg(m_minVal+(m_maxVal-m_minVal)*m_verticalIndicatorPositionPercent,0,'f',precision);
		renderText(leftSide ? x-strMetrics.width(valueStr)-c_innerMargin : x+c_innerMargin, h-y, valueStr, m_renderingFont);
	}
}

void ccHistogramWindow::mousePressEvent(QMouseEvent *event)
{
	mouseMoveEvent(event);
}

void ccHistogramWindow::mouseMoveEvent(QMouseEvent *event)
{
	bool actionDetected = false;
	if (event->buttons() & Qt::LeftButton)
	{
		int x = event->x();
		int y = height()-event->y();

		if (m_numberOfClassesCanBeChanged)
		{
			if (m_histoValues.size() > 4) //"minus" button
			{
				if (y >= m_yMinusButton-m_buttonSize && y <= m_yMinusButton)
				{
					if (x >= m_xMinusButton && x <= m_xMinusButton+m_buttonSize)
					{
						setNumberOfClasses(m_histoValues.size()-4);
						actionDetected = true;
					}
				}
			}

			if (!actionDetected)
			{
				if (y >= m_yPlusButton-m_buttonSize && y <= m_yPlusButton) //"plus" button
				{
					if (x >= m_xPlusButton && x <= m_xPlusButton+m_buttonSize)
					{
						setNumberOfClasses(m_histoValues.size()+4);
						actionDetected = true;
					}
				}
			}
		}

		if (!actionDetected) //click anywhere else?
		{
			if (x>m_roi[0] && x<=m_roi[2] && y>m_roi[1] && y<=m_roi[3])
			{
				m_drawVerticalIndicator = true;
				if (m_roi[2] > m_roi[0])
				{
					int verticalIndicatorPosition = static_cast<int>(m_histoValues.size()) * (x-m_roi[0]) / (m_roi[2]-m_roi[0]);
					m_verticalIndicatorPositionPercent = static_cast<double>(verticalIndicatorPosition) / m_histoValues.size();
				}
				else
				{
					m_verticalIndicatorPositionPercent = 0;
				}
				actionDetected = true;
			}
		}
	}
	else event->ignore();

	if (actionDetected)
		updateGL();
}

void ccHistogramWindow::wheelEvent(QWheelEvent* e)
{
	if (!m_numberOfClassesCanBeChanged)
	{
		e->ignore();
		return;
	}

	if (e->delta() < 0)
	{
		if (m_histoValues.size() > 4)
		{
			setNumberOfClasses(std::max<size_t>(4,m_histoValues.size()-4));
			updateGL();
		}
	}
	else //if (e->delta() > 0)
	{
		setNumberOfClasses(m_histoValues.size()+4);
		updateGL();
	}

	e->accept();
}

ccHistogramWindow::DisplayParameters::DisplayParameters()
	: useDefaultParameters(true)
	, useGradientColor(true)
{
	backgroundColor[0] = backgroundColor[1] = backgroundColor[2] = 255;
	defaultColor[0] = defaultColor[1] = defaultColor[2] = 0;
}

const unsigned char* ccHistogramWindow::DisplayParameters::getBackgroundColor() const
{
	if (useDefaultParameters)
		return ccGui::Parameters().histBackgroundCol;
	else
		return backgroundColor;
}

const unsigned char* ccHistogramWindow::DisplayParameters::getDefaultColor() const
{
	if (useDefaultParameters)
		return ccGui::Parameters().textDefaultCol;
	else
		return defaultColor;
}
