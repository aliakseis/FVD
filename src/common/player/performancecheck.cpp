#include "performancecheck.h"

#include "widgetdisplay.h"
//#include "opengldisplay.h"
#include "ffmpegdecoder.h"


int performanceCheck()
{
	// generate 10 pictures 2000x2000 pixels (random colors)
	const int picnumber = 10;
	const int pixels_x = 555;
	const int pixels_y = 555;

	const int picture_size = pixels_x * pixels_y * 3;
	uint8_t* pictures[picnumber];
	FFmpegDecoder::VideoFrame test_frames[picnumber];
	for (int i = 0; i < picnumber; ++i)
	{
		pictures[i] = new uint8_t[picture_size];
		// will be black without it
		/*
		for(int j = 0; j < picture_size; ++j)
		{
			// random pixels
			pictures[i][j] = rand() % 256;
		}
		*/
		test_frames[i].m_width = pixels_x;
		test_frames[i].m_height = pixels_y;
		test_frames[i].m_image = pictures[i];
	}
	int64_t start_time;
	int64_t end_time;
	int time_widgets;
	int out_result = 0;


	// Test qwidget
	WidgetDisplay* widget_w = new WidgetDisplay(NULL);
	start_time = av_gettime();
	for (int i = 0; i < picnumber; ++i)
	{
		//	widget_w->renderFrame(test_frames[i]);
	}
	end_time = av_gettime();
	delete widget_w;
	out_result = 1; // First one
	time_widgets = end_time - start_time;
	qDebug() << "[FFMPEG][TEST] Widgets: " << time_widgets;


	// Test Opengl widget
	// 	OpenGLDisplay* widget_o = new OpenGLDisplay(NULL);
	// 	start_time = av_gettime();
	// 	for (int i = 0; i < picnumber; ++i) {
	// 		widget_o->renderFrame(test_frames[i]);
	// 	}
	// 	end_time = av_gettime();
	// 	delete widget_o;
	// 	if (end_time - start_time < time_widgets)
	// 		out_result = 2; // Second one
	// 	time_widgets = end_time - start_time;
	// 	qDebug() << "[FFMPEG][TEST] OpenGL: " << time_widgets;
	//
	// 	for (int i = 0; i < picnumber; ++i)
	// 		delete pictures[i];
	//
	return out_result;
}