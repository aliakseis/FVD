#include <cmath>
#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>
#include <QFileInfo>
#include <fstream>

#include "mainwindow.h"
#include "performancecheck.h"

MainWindow::MainWindow(QWidget* parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow)
{
	ui->setupUi(this);

	qDebug() << "Performance choise: " << performanceCheck();

	decoder1.setFrameListener(ui->labelVideoFrame);
	//decoder2.setFrameListener(ui->labelVideoFrame_2);
	connect(ui->audioSlider, SIGNAL(valueChanged(int)), &decoder1, SLOT(setVolume(int)));
	//connect(ui->audioSlider, SIGNAL(valueChanged(int)), &decoder2, SLOT(setVolume(int)));
}

MainWindow::~MainWindow()
{
}

void MainWindow::changeEvent(QEvent* e)
{
	QMainWindow::changeEvent(e);
	switch (e->type())
	{
	case QEvent::LanguageChange:
		ui->retranslateUi(this);
		break;
	default:
		break;
	}
}

void MainWindow::on_actionQuit_triggered()
{
	close();
}

void MainWindow::image2Pixmap(QImage& img, QPixmap& pixmap)
{
	// Convert the QImage to a QPixmap for display
	pixmap = QPixmap(img.size());
	QPainter painter;
	painter.begin(&pixmap);
	painter.drawImage(0, 0, img);
	painter.end();
}





/******************************************************************************
*******************************************************************************
* Decoder demo   Decoder demo   Decoder demo   Decoder demo   Decoder demo
*******************************************************************************
******************************************************************************/

void MainWindow::on_actionLoad_video_triggered()
{
	// Prompt a video to load
	QString fileName = QFileDialog::getOpenFileName(this, "Load Video", QString(), "Video (*.avi *.asf *.mpg *.mp4 *.mkv *.wmv *.webm *.part *.crdownload)");
	if (!fileName.isNull())
	{
		loadVideo(fileName);
	}
}

/**
  Prompts the user for the video to load, and display the first frame
**/
// good open

//#if 0
void MainWindow::loadVideo(QString fileName)
{
	//QImage image = decoder1.getRandomFrame(fileName);
	//qDebug() << image.size();
	//ui->labelVideoFrame_2->setPixmap(QPixmap::fromImage(image));
	decoder1.openFile(fileName);
	//decoder2.openFile(fileName);
	decoder1.play();
	//decoder2.play();
}
//#endif

#if 0
void MainWindow::loadVideo(QString fileName)
{
	m_fileName = fileName;
	m_mockDownloadThread = new MockDownloadThread(this);
	m_mockDownloadThread->start();
}
#endif

void MainWindow::MockDownloadThread::run()
{
	MainWindow* pr = (MainWindow*)parent();

	std::ifstream in(pr->m_fileName.toStdString().c_str(), std::ios::binary);
	QString newFile = QFileInfo(pr->m_fileName).absoluteDir().absolutePath() + "/" + pr->ui->outFileName->text();
	qDebug() << newFile;
	std::ofstream out(newFile.toStdString().c_str(), std::ios::binary);

	char buffer[1];
	int seconds = pr->ui->downloadSpeedPer->text().toInt();
	int limit = 0;
	if (!pr->ui->startTime->text().isEmpty())
	{
		limit = pr->ui->startTime->text().toInt();
	}
	while (!in.eof())
	{
		if (seconds > 0)
		{
			if (!pr->ui->startTime->text().isEmpty())
			{
				if (in.tellg() >= limit)
				{
					Sleep(seconds);
				}
			}
			else
			{
				Sleep(seconds);
			}
		}
		in.read(buffer, 1);
		if (!in.fail())
		{
			out.write(buffer, 1);
		}
		if (in.tellg() >= limit)
		{
			//qDebug() << "Bytes written: " << in.tellg();
			pr->ui->byteCounter->setText(QString::number(in.tellg()));
		}
	}
	in.close();
	out.close();
}



void MainWindow::errLoadVideo()
{
	QMessageBox::critical(this, "Error", "Load a video first");
}
bool MainWindow::checkVideoLoadOk()
{
	// if(decoder.isOk()==false)
	//{
	// errLoadVideo();
	//return false;
	//}
	return true;
}

/**
  Decode and display a frame
**/
void MainWindow::displayFrame()
{
	//ui->labelVideoFrame->displayFrame(decoder.getCurrentFrame());
}


void MainWindow::nextFrame()
{

}

/**
  Display next frame
**/
void MainWindow::on_pushButtonNextFrame_clicked()
{
	decoder1.pause();
	//decoder2.pause();
}




void MainWindow::on_pushButtonSeekFrame_clicked()
{
	// Check we've loaded a video successfully
	if (!checkVideoLoadOk())
	{
		return;
	}

	bool ok;

	int frame = ui->lineEditFrame->text().toInt(&ok);
	if (!ok || frame < 0)
	{
		QMessageBox::critical(this, "Error", "Invalid frame number");
		return;
	}

	// Seek to the desired frame
	decoder1.seekFrame(frame);
	//decoder2.seekFrame(frame);
	// Display the frame
	//displayFrame();

}


void MainWindow::on_pushButtonSeekMillisecond_clicked()
{
	// Check we've loaded a video successfully
	if (!checkVideoLoadOk())
	{
		return;
	}

	bool ok;

	int ms = ui->lineEditMillisecond->text().toInt(&ok);

	// Seek to the desired frame
	decoder1.seekMs(ms);
	//decoder2.seekMs(ms);

}





/******************************************************************************
*******************************************************************************
* Encoder demo   Encoder demo   Encoder demo   Encoder demo   Encoder demo
*******************************************************************************
******************************************************************************/

/**
  Prompts the user for a file
  Create the file
  Pass the file to the video generation function (alternatively the file name could be passed)
**/
void MainWindow::on_actionSave_synthetic_video_triggered()
{
	QString title("Save a synthetic video");
	QString fileName = QFileDialog::getSaveFileName(this, title, QString(), "Video (*.avi *.asf *.mpg)");
	if (!fileName.isNull())
	{
		GenerateSyntheticVideo(fileName);
	}
}

void MainWindow::on_actionSave_synthetic_variable_frame_rate_video_triggered()
{
	QString title("Save a synthetic video with variable frame rate");
	QString fileName = QFileDialog::getSaveFileName(this, title, QString(), "Video (*.avi *.asf *.mpg *.mp4)");
	if (!fileName.isNull())
	{
		GenerateSyntheticVideo(fileName, true);
	}
}


void MainWindow::GenerateSyntheticVideo(QString filename, bool vfr)
{
	int width = 640;
	int height = 480;
	int bitrate = 1000000;
	int gop = 20;
	int fps = 25;

	// The image on which we draw the frames
	QImage frame(width, height, QImage::Format_RGB32);   // Only RGB32 is supported

	// A painter to help us draw
	QPainter painter(&frame);
	painter.setBrush(Qt::red);
	painter.setPen(Qt::white);

	// Create the encoder
	//QVideoEncoder encoder;
	//if(!vfr)
	// encoder.createFile(filename,width,height,bitrate,gop,fps);        // Fixed frame rate
	//else
	// encoder.createFile(filename,width,height,bitrate*1000/fps,gop,1000);  // For variable frame rates: set the time base to e.g. 1ms (1000fps),
	// and correct the bitrate according to the expected average frame rate (fps)

	QEventLoop evt;      // we use an event loop to allow for paint events to show on-screen the generated video

	// Generate a few hundred frames
	int size = 0;
	int maxframe = 500;
	unsigned pts = 0;
	for (int i = 0; i < maxframe; i++)
	{
		// Clear the frame
		painter.fillRect(frame.rect(), Qt::red);

		// Draw a moving square
		painter.fillRect(width * i / maxframe, height * i / maxframe, 30, 30, Qt::blue);

		// Frame number
		painter.drawText(frame.rect(), Qt::AlignCenter, QString("Frame %1\nLast frame was %2 bytes").arg(i).arg(size));

		// Display the frame, and processes events to allow for screen redraw
		QPixmap p;
		image2Pixmap(frame, p);
		//ui->labelVideoFrame->setPixmap(p);
		evt.processEvents();

		//      if(!vfr)
		//       size=encoder.encodeImage(frame);                      // Fixed frame rate
		//  else
		//{
		// Variable frame rate: the pts of the first frame is 0,
		// subsequent frames slow down
		//pts += sqrt(i);
		//if(i==0)
		//  size=encoder.encodeImagePts(frame,0);
		// else
		//    size=encoder.encodeImagePts(frame,pts);
		//}

		printf("Encoded: %d\n", size);
	}

	//encoder.close();

}
