//
//  PlexJobs.cpp
//  Plex Home Theater
//
//  Created by Tobias Hieta on 2013-08-14.
//
//

#include "PlexJobs.h"
#include "FileSystem/PlexDirectory.h"

#include "FileSystem/PlexFile.h"

#include "TextureCache.h"
#include "File.h"
#include "utils/Crc32.h"
#include "PlexFile.h"
#include "video/VideoInfoTag.h"
#include "Stopwatch.h"
#include "PlexUtils.h"

////////////////////////////////////////////////////////////////////////////////
bool CPlexHTTPFetchJob::DoWork()
{
  return m_http.Get(m_url.Get(), m_data);
}

////////////////////////////////////////////////////////////////////////////////
bool CPlexHTTPFetchJob::operator==(const CJob* job) const
{
  const CPlexHTTPFetchJob *f = static_cast<const CPlexHTTPFetchJob*>(job);
  return m_url.Get() == f->m_url.Get();
}

////////////////////////////////////////////////////////////////////////////////
bool CPlexDirectoryFetchJob::DoWork()
{
  return m_dir.GetDirectory(m_url.Get(), m_items);
}

////////////////////////////////////////////////////////////////////////////////
boost::unordered_map<std::string,unsigned long> CPlexCachedDirectoryFetchJob::m_urlHash;
CCriticalSection CPlexCachedDirectoryFetchJob::m_hashMaplock;

unsigned long CPlexCachedDirectoryFetchJob::GetHashFromCache(const CURL& url)
{
  CSingleLock lock(m_hashMaplock);
  return m_urlHash[url.Get()];
}

////////////////////////////////////////////////////////////////////////////////
void CPlexCachedDirectoryFetchJob::SetCacheHash(const CURL& url, unsigned long hash)
{
  CSingleLock lock(m_hashMaplock);
  m_urlHash[url.Get()] = hash;
}

////////////////////////////////////////////////////////////////////////////////
bool CPlexCachedDirectoryFetchJob::DoWork()
{
  bool bResult = true;
  CStopWatch timer;

  timer.StartZero();

  CLog::Log(LOGDEBUG,"CPlexCachedDirectoryFetchJob::DoWork, Starting Fetch");

  // 1 - grab the url XML content
  if (boost::contains(m_url.GetFileName(), "library/metadata"))
    m_url.SetOption("checkFiles", "1");

  if (m_url.HasProtocolOption("containerSize"))
  {
    m_url.SetOption("X-Plex-Container-Size", m_url.GetProtocolOption("containerSize"));
    m_url.RemoveProtocolOption("containerSize");
  }
  if (m_url.HasProtocolOption("containerStart"))
  {
    m_url.SetOption("X-Plex-Container-Start", m_url.GetProtocolOption("containerStart"));
    m_url.RemoveProtocolOption("containerStart");
  }

  bool httpSuccess;
  XFILE::CPlexFile file;
  CStdString xmlData;

  httpSuccess = file.Get(m_url.Get(), xmlData);

  if (!httpSuccess)
  {
    CLog::Log(LOGDEBUG, "CPlexCachedDirectoryFetchJob::DoWork failed to fetch data from %s: %ld", m_url.Get().c_str(), file.GetLastHTTPResponseCode());
    if (file.GetLastHTTPResponseCode() == 500)
    {
      /* internal server error, we should handle this .. */
    }
    return false;
  }

  // 2 - Compute the URL hash
  m_newHash = PlexUtils::GetFastHash(xmlData);
  m_oldHash = GetHashFromCache(m_url);

  CLog::Log(LOGDEBUG, "CPlexCachedDirectoryFetchJob::DoWork New Hash = %X, Old Hash = %X",m_newHash,m_oldHash);
  if (m_newHash != m_oldHash)
  {
    CLog::Log(LOGDEBUG, "CPlexCachedDirectoryFetchJob::DoWork detected that section '%s'' content has changed, refreshing ...",m_url.Get().c_str());
    // refetch directory if content has changed
    bResult = CPlexDirectoryFetchJob::DoWork();

    if (bResult)
      SetCacheHash(m_url,m_newHash);
  }
  else
    CLog::Log(LOGDEBUG, "CPlexCachedDirectoryFetchJob::DoWork did not find any change in section '%s'",m_url.Get().c_str());

  CLog::Log(LOGDEBUG,"CPlexCachedDirectoryFetchJob::DoWork, Fetch took %f",timer.GetElapsedSeconds());
  return bResult;
}

////////////////////////////////////////////////////////////////////////////////
bool CPlexMediaServerClientJob::DoWork()
{
  XFILE::CPlexFile file;
  bool success = false;
  
  if (m_verb == "PUT")
    success = file.Put(m_url.Get(), m_data);
  else if (m_verb == "GET")
    success = file.Get(m_url.Get(), m_data);
  else if (m_verb == "DELETE")
    success = file.Delete(m_url.Get(), m_data);
  else if (m_verb == "POST")
    success = file.Post(m_url.Get(), m_postData, m_data);
  
  return success;
}

////////////////////////////////////////////////////////////////////////////////////////
bool CPlexVideoThumbLoaderJob::DoWork()
{
  if (!m_item->IsPlexMediaServer())
    return false;

  CStdStringArray art;
  art.push_back("smallThumb");
  art.push_back("smallPoster");
  art.push_back("smallGrandparentThumb");

  int i = 0;
  BOOST_FOREACH(CStdString artKey, art)
  {
    if (m_item->HasArt(artKey) &&
        !CTextureCache::Get().HasCachedImage(m_item->GetArt(artKey)))
      CTextureCache::Get().BackgroundCacheImage(m_item->GetArt(artKey));

    if (ShouldCancel(i++, art.size()))
      return false;
  }

  return true;
}

using namespace XFILE;

////////////////////////////////////////////////////////////////////////////////////////
bool
CPlexDownloadFileJob::DoWork()
{
  CCurlFile http;
  CFile file;
  CURL theUrl(m_url);
  http.SetRequestHeader("X-Plex-Client", PLEX_TARGET_NAME);

  if (!file.OpenForWrite(m_destination, true))
  {
    CLog::Log(LOGWARNING, "[DownloadJob] Couldn't open file %s for writing", m_destination.c_str());
    return false;
  }

  if (http.Open(theUrl))
  {
    CLog::Log(LOGINFO, "[DownloadJob] Downloading %s to %s", m_url.c_str(), m_destination.c_str());

    bool done = false;
    bool failed = false;
    int64_t read;
    int64_t leftToDownload = http.GetLength();
    int64_t total = leftToDownload;

    while (!done)
    {
      char buffer[4096];
      read = http.Read(buffer, 4096);
      if (read > 0)
      {
        leftToDownload -= read;
        file.Write(buffer, read);
        done = ShouldCancel(total-leftToDownload, total);
        if(done) failed = true;
      }
      else if (read == 0)
      {
        done = true;
        failed = total == 0;
        continue;
      }

      if (total == 0)
        done = true;
    }

    CLog::Log(LOGINFO, "[DownloadJob] Done with the download.");

    http.Close();
    file.Close();

    return !failed;
  }

  CLog::Log(LOGWARNING, "[DownloadJob] Failed to download file.");
  return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlexThemeMusicPlayerJob::DoWork()
{
  CStdString themeMusicUrl = m_item.GetProperty("theme").asString();
  if (themeMusicUrl.empty())
    return false;

  Crc32 crc;
  crc.ComputeFromLowerCase(themeMusicUrl);

  CStdString hex;
  hex.Format("%08x", (unsigned int)crc);

  m_fileToPlay = "special://masterprofile/ThemeMusicCache/" + hex + ".mp3";

  if (!XFILE::CFile::Exists(m_fileToPlay))
  {
    CPlexFile plex;
    CFile localFile;

    if (!localFile.OpenForWrite(m_fileToPlay, true))
    {
      CLog::Log(LOGWARNING, "CPlexThemeMusicPlayerJob::DoWork failed to open %s for writing.", m_fileToPlay.c_str());
      return false;
    }

    bool failed = false;

    if (plex.Open(themeMusicUrl))
    {
      bool done = false;
      int64_t read = 0;

      while(!done)
      {
        char buffer[4096];
        read = plex.Read(buffer, 4096);
        if (read > 0)
        {
          localFile.Write(buffer, read);
          done = ShouldCancel(0, 0);
          if (done) failed = true;
        }
        else if (read == 0)
        {
          done = true;
          continue;
        }
      }
    }

    CLog::Log(LOGDEBUG, "CPlexThemeMusicPlayerJob::DoWork cached %s => %s", themeMusicUrl.c_str(), m_fileToPlay.c_str());

    plex.Close();
    localFile.Close();

    return !failed;
  }
  else
    return true;
}
