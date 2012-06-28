/*
 * download.cpp
 *
 *  Created on: 23.06.2012
 *      Author: Sergey Gagarin a.k.a. Seg@
 */

#include "download.h"

#include <SDL.h>

#include "display.h"
#include "localevent.h"
#include "zzlib.h"
#include "unzip.h"
#include "settings.h"

#define CURL_STATICLIB
#include <stdio.h>
#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>
#include <string>

#include <bps/bps.h>
#include <bps/dialog.h>
#include <bps/navigator.h>
#include <screen/screen.h>
#include <stdio.h>
#include <stdlib.h>

static screen_context_t screen_ctx;
static screen_window_t screen_win;
dialog_instance_t alert_dialog = 0;


/**
 * Use the PID to set the window group id.
 */
char *
get_window_group_id()
{
    static char s_window_group_id[16] = "";

    if (s_window_group_id[0] == '\0') {
        snprintf(s_window_group_id, sizeof(s_window_group_id), "%d", getpid());
    }

    return s_window_group_id;
}

/**
 * Set up a basic screen, so that the navigator will
 * send window state events when the window state changes.
 *
 * @return @c EXIT_SUCCESS or @c EXIT_FAILURE
 */
int
setup_screen()
{
    if (screen_create_context(&screen_ctx, SCREEN_APPLICATION_CONTEXT) != 0) {
        return EXIT_FAILURE;
    }

    if (screen_create_window(&screen_win, screen_ctx) != 0) {
        screen_destroy_context(screen_ctx);
        return EXIT_FAILURE;
    }

    int usage = SCREEN_USAGE_NATIVE;
    if (screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_USAGE, &usage) == 0)
    {
      if (screen_create_window_buffers(screen_win, 1) == 0)
      {
        if (screen_create_window_group(screen_win, get_window_group_id()) == 0)
        {
          screen_buffer_t buff;
          if (screen_get_window_property_pv(screen_win, SCREEN_PROPERTY_RENDER_BUFFERS, (void**)&buff) == 0)
          {
            int buffer_size[2];
            if (screen_get_buffer_property_iv(buff, SCREEN_PROPERTY_BUFFER_SIZE, buffer_size) == 0)
            {
              int attribs[1] = {SCREEN_BLIT_END};
              if (screen_fill(screen_ctx, buff, attribs) == 0)
              {
                int dirty_rects[4] = {0, 0, buffer_size[0], buffer_size[1]};
                if (screen_post_window(screen_win, buff, 1, (const int*)dirty_rects, 0) == 0)
                    return EXIT_SUCCESS;
              }
            }
          }
        }
      }
    }

    screen_destroy_window(screen_win);
    screen_destroy_context(screen_ctx);
    return EXIT_FAILURE;
}

static
void show_alert_dialog()
{
  if (alert_dialog)
    return;

  if (dialog_create_alert(&alert_dialog) != BPS_SUCCESS)
  {
    fprintf(stderr, "Failed to create alert dialog.");
    return;
  }

  const char* cancel_button_context = "Canceled";
  const char* ok_button_context = "Agreed";

  if ((dialog_set_alert_message_text(alert_dialog, "You need to download Heroes 2 Demo data first (~20Mb)!\nAre you sure you want to proceed?") != BPS_SUCCESS) ||
      (dialog_add_button(alert_dialog, DIALOG_OK_LABEL, true, ok_button_context, true) != BPS_SUCCESS) ||
      (dialog_add_button(alert_dialog, DIALOG_CANCEL_LABEL, true, cancel_button_context, true) != BPS_SUCCESS) ||
      (dialog_show(alert_dialog) != BPS_SUCCESS) )
  {
    fprintf(stderr, "Failed to add button to alert dialog.");
    dialog_destroy(alert_dialog);
    alert_dialog = 0;
  }
}

/**
 * Handle a dialog response.
 */
static void
handle_dialog_response(bps_event_t *event)
{
  if (event == NULL)
    return;

  int selectedIndex = dialog_event_get_selected_index(event);
  const char* label = dialog_event_get_selected_label(event);
  const char* context = dialog_event_get_selected_context(event);

  char output[1024];
  snprintf(output, 1024, "Selected Index: %d, Label: %s, Context: %s\n",
        selectedIndex, label?label:"n/a", context?(char*)context:"n/a");
  fprintf(stderr, output);

  dialog_destroy(alert_dialog);
  alert_dialog = 0;
}


size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  size_t written;
  written = fwrite(ptr, size, nmemb, stream);
  return written;
}

void drawCenteredImage(const char* szFileName)
{
  Display & display = Display::Get();
  Surface srf;
  if (srf.Load(szFileName))
  {
    display.Fill(srf.MapRGB(0, 0, 0));
    srf.Blit((display.w() - srf.w()) / 2, (display.h() - srf.h()) / 2, display);
    display.Flip();
  }
}

void unpackCurrentFile( unzFile unzFile, unsigned long size, const char* i_szFname )
{
  fprintf(stderr, "Unpacking to %s\n", i_szFname);
  unzOpenCurrentFile(unzFile);
  std::vector<char> buf;
  buf.resize(size);
  unzReadCurrentFile(unzFile, &(buf[0]), size);
  unzCloseCurrentFile(unzFile);
  FILE* f = fopen(i_szFname, "wb");
  if (!f)
    fprintf(stderr, "failed to open file for writing");
  else
  {
    fwrite(&(buf[0]), size, 1, f);
    fclose(f);
  }
}

void loadDemo()
{
  /*
  int exit_application = 0;
  bps_initialize();
  if (setup_screen() != EXIT_SUCCESS)
  {
    fprintf(stderr, "Unable to initialize screen.");
    exit(0);
  }
  navigator_request_events(0);
  dialog_request_events(0);
  show_alert_dialog();

  while (!exit_application)
  {
    bps_event_t *event = NULL;
    bps_get_event(&event, -1);

    if (event)
    {
      if (bps_event_get_domain(event) == dialog_get_domain())
        handle_dialog_response(event);

      if (bps_event_get_domain(event) == navigator_get_domain())
      {
        unsigned int code = bps_event_get_code(event);
        switch(code)
        {
        case NAVIGATOR_EXIT:
          exit_application = 1; break;
        case NAVIGATOR_SWIPE_DOWN:
          show_alert_dialog();
          break;
        }
      }
    }
  }

  if (alert_dialog)
    dialog_destroy(alert_dialog);
  bps_shutdown();
  screen_destroy_window(screen_win);
  screen_destroy_context(screen_ctx);
  */

  fprintf(stderr, "Loading demo...\n");

  LocalEvent & le = LocalEvent::Get();

  drawCenteredImage("app/native/assets/download1_request.png");
  while(le.HandleEvents() && !le.KeyPress() && !le.MouseClickLeft());

  drawCenteredImage("app/native/assets/download2_progress.png");

  fprintf(stderr, "Displayed...\n");

  const char *url = "http://downloads.pcworld.com/pub/new/fun_and_games/adventure_strategy/h2demo.zip";
  std::string outfilename = "data/h2demo.zip";
  fprintf(stderr, "Trying to download DEMO to %s\n", outfilename.c_str());
  CURL *curl = curl_easy_init();
  if (!curl)
  {
    fprintf(stderr, "Failed to initialize curl\n");
    return;
  }

  fprintf(stderr, "Curling...\n");
  remove(outfilename.c_str());
  FILE *fp = fopen(outfilename.c_str(),"wb");
  if (fp == NULL)
  {
    fprintf(stderr, "Failed to open the file\n");
    return;
  }
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
  CURLcode res = curl_easy_perform(curl);
  curl_easy_cleanup(curl);
  fclose(fp);
  if (res != CURLE_OK)
  {
    fprintf(stderr, "Curle get failed\n");
    return;
  }
  fprintf(stderr, "Curled...\n");

  drawCenteredImage("app/native/assets/download3_extract.png");

  unzFile unzFile = unzOpen(outfilename.c_str());
  if (!unzFile)
  {
    fprintf(stderr, "Failed to open downloaded file\n");
    return;
  }

  for ( int rc = unzGoToFirstFile(unzFile); rc == UNZ_OK; rc = unzGoToNextFile(unzFile) )
  {
    char fileName[256];
    unz_file_info file_info;
    unzGetCurrentFileInfo(unzFile, &file_info, fileName, 256, 0, 0, 0, 0);
    fprintf(stderr, "%s\n", fileName);

    if ( stricmp(fileName, "DATA/HEROES2.AGG") == 0 )
      unpackCurrentFile(unzFile, file_info.uncompressed_size, (Settings::GetWriteableDir("data") + SEPARATOR + "heroes2.agg").c_str());
    else
    if ( stricmp(fileName, "MAPS/BROKENA.MP2") == 0 )
      unpackCurrentFile(unzFile, file_info.uncompressed_size, (Settings::GetWriteableDir("maps") + SEPARATOR + "brokena.mp2").c_str());
  }

  unzClose(unzFile);

  drawCenteredImage("app/native/assets/download4_done.png");
  while(le.HandleEvents() && !le.KeyPress() && !le.MouseClickLeft());
}
