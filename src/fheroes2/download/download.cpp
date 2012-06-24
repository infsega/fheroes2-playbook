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
