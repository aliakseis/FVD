#pragma once

#include "utilities/translation.h"

namespace
{

using namespace utilities;

const char DEFAULT_USERAGENT[]		=	"Mozilla/5.0 (Windows NT 6.1; WOW64; rv:13.0) Gecko/20100101 Firefox/13.0.1";
const char PROJECT_ICON[]			=	":/downloader.ico";

Tr::Translation DOWNLOAD_TO_LABEL 				=	Tr::translate("Preferences", "Download to:", "Download to dialog header");
Tr::Translation DOWNLOAD_BUTTON 				=	Tr::translate("SearchResultForm", "Download", "Download button in search results");

Tr::Translation TREEVIEW_TITLE_HEADER			=	Tr::translate("SearchResultForm", "Title");
Tr::Translation TREEVIEW_DESCRIPTION_HEADER		=	Tr::translate("SearchResultForm", "Description");
Tr::Translation TREEVIEW_PROGRESS_HEADER		=	Tr::translate("SearchResultForm", "Progress");
Tr::Translation TREEVIEW_SPEED_HEADER			=	Tr::translate("SearchResultForm", "Speed");
Tr::Translation SEARCH_TREEVIEW_STATUS_HEADER	=	Tr::translate("SearchResultForm", "Action/Status");
Tr::Translation DOWNLOAD_TREEVIEW_STATUS_HEADER	=	Tr::translate("SearchResultForm", "Status");
Tr::Translation TREEVIEW_SIZE_HEADER			=	Tr::translate("SearchResultForm", "Size");
Tr::Translation TREEVIEW_LENGTH_HEADER			=	Tr::translate("SearchResultForm", "Length");
Tr::Translation SEARCH_FAILED_MESSAGE			=	Tr::translate("SearchResultForm", "Search on %1 failed with error %2");

Tr::Translation TREEVIEW_DOWNLOADING_STATUS		=	Tr::translate("DownloadsForm", "Downloading");
Tr::Translation TREEVIEW_PPEPARING_STATUS		=	Tr::translate("DownloadsForm", "Preparing...");
Tr::Translation TREEVIEW_QUEUED_STATUS			=	Tr::translate("DownloadsForm", "Queued");
Tr::Translation TREEVIEW_WAITING_STATUS			=	Tr::translate("DownloadsForm", "Waiting...");
Tr::Translation TREEVIEW_PAUSED_STATUS			=	Tr::translate("DownloadsForm", "Paused");
Tr::Translation TREEVIEW_CANSELED_STATUS		=	Tr::translate("DownloadsForm", "Canceled");
Tr::Translation TREEVIEW_ERROR_STATUS			=	Tr::translate("DownloadsForm", "Error");
Tr::Translation TREEVIEW_FINISHED_STATUS		=	Tr::translate("DownloadsForm", "Completed");

Tr::Translation LIBRARY_SEARCH_INVITATION		=	Tr::translate("LibraryForm", "Type search query here...");

Tr::Translation LANGUAGE_NAME					=   Tr::translate("Preferences", "English (English)", "Language name in preferences");

Tr::Translation ABOUT_TITLE						=   Tr::translate("AboutDialog", "About %1");
Tr::Translation VERSION_TEXT 					=	Tr::translate("AboutDialog", "Version %1");
Tr::Translation COPYRIGHT_TEXT 					=	Tr::translate("AboutDialog", "Copyright 2013 %1, All Rights reserved");
Tr::Translation ABOUT_HELP						=	Tr::translate("AboutDialog", "Help");
Tr::Translation ABOUT_FEEDBACK					=	Tr::translate("AboutDialog", "Feedback");

Tr::Translation CREATEDIR_ERROR					=	Tr::translate("MainWindow", "<b>Cannot create directory \"%1\"</b>");
Tr::Translation CREATEFILE_ERROR				=	Tr::translate("MainWindow", "<b>Cannot create file \"%1\"</b>");
}
