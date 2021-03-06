//
//  PlexJobs.h
//  Plex Home Theater
//
//  Created by Tobias Hieta on 2013-08-14.
//
//

#ifndef __Plex_Home_Theater__PlexJobs__
#define __Plex_Home_Theater__PlexJobs__

#include "utils/Job.h"
#include "URL.h"
#include "filesystem/CurlFile.h"
#include "FileItem.h"
#include "guilib/GUIMessage.h"
#include "Client/PlexMediaServerClient.h"
#include "FileSystem/PlexDirectory.h"
#include "threads/CriticalSection.h"

#ifdef TARGET_RASPBERRY_PI
#include "dialogs/GUIDialogKaiToast.h"
#include "PlexAutoUpdate.h"
#include "filesystem/SpecialProtocol.h"
#include "xbmc/Util.h"
#endif

////////////////////////////////////////////////////////////////////////////////////////
class CPlexHTTPFetchJob : public CJob
{
  public:
    CPlexHTTPFetchJob(const CURL &url, CPlexServerPtr server = CPlexServerPtr()) : CJob(), m_url(url), m_server(server) {};
  
    bool DoWork();
    void Cancel() { m_http.Cancel(); }
    virtual bool operator==(const CJob* job) const;
  
    XFILE::CCurlFile m_http;
    CStdString m_data;
    CURL m_url;
    CPlexServerPtr m_server;
};

////////////////////////////////////////////////////////////////////////////////////////
class CPlexDirectoryFetchJob : public CJob
{
public:
  CPlexDirectoryFetchJob(const CURL &url, CPlexDirectoryCache::CacheStrategies Startegy = CPlexDirectoryCache::CACHE_STRATEGY_ITEM_COUNT) : CJob(), m_url(url) { m_dir.SetCacheStrategy(Startegy);}
  
  virtual bool operator==(const CJob* job) const
  {
    const CPlexDirectoryFetchJob *fjob = static_cast<const CPlexDirectoryFetchJob*>(job);
    if (fjob && fjob->m_url.Get() == m_url.Get())
      return true;
    return false;
  }
  
  virtual const char* GetType() const { return "plexdirectoryfetch"; }
  
  virtual bool DoWork();

  virtual void Cancel()
  {
    m_dir.CancelDirectory();
  }
  
  XFILE::CPlexDirectory m_dir;
  CFileItemList m_items;
  CURL m_url;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
class CPlexPlayQueueFetchJob : public CPlexDirectoryFetchJob
{
public:
  CPlexPlayQueueFetchJob(const CURL& url, bool startPlaying = true)
    : CPlexDirectoryFetchJob(url), m_startPlaying(startPlaying)
  {

  }

  bool m_startPlaying;
};

////////////////////////////////////////////////////////////////////////////////////////
class CPlexSectionFetchJob : public CPlexDirectoryFetchJob
{
public:
  CPlexSectionFetchJob(const CURL& url, int contentType) : CPlexDirectoryFetchJob(url,CPlexDirectoryCache::CACHE_STRATEGY_ALWAYS), m_contentType(contentType) { }
  int m_contentType;
};

////////////////////////////////////////////////////////////////////////////////////////
class CPlexMediaServerClientJob : public CJob
{
public:
  CPlexMediaServerClientJob(CURL command, const std::string verb = "GET", const CGUIMessage &msg = CGUIMessage(0, 0, 0, 0), int error = 0) :
    m_url(command), m_verb(verb), m_msg(msg), m_errorMsg(error) {}
  
  virtual bool DoWork();
  
  CURL m_url;
  std::string m_verb;
  CStdString m_data;
  CGUIMessage m_msg;
  CStdString m_postData;
  int m_errorMsg;
  XFILE::CPlexFile m_http;

  virtual void Cancel() { m_http.Cancel(); }

  /* set this to the shared ptr if we are calling this from the
   * mediaServerClient, otherwise just don't bother */
  CPlexMediaServerClientPtr m_mediaServerClient;

  virtual bool operator==(const CJob* job) const
  {
    CPlexMediaServerClientJob *oJob = (CPlexMediaServerClientJob*)job;
    if (oJob->m_url.Get() == m_url.Get())
      return true;
    return false;
  }
};

////////////////////////////////////////////////////////////////////////////////////////
class CPlexVideoThumbLoaderJob : public CJob
{
public:
  CPlexVideoThumbLoaderJob(CFileItemPtr item) : m_item(item) {}

  virtual bool DoWork();

  virtual bool operator==(const CJob* job) const
  {
    CPlexVideoThumbLoaderJob *ljob = (CPlexVideoThumbLoaderJob*)job;
    return m_item == ljob->m_item;
  }

  CFileItemPtr m_item;
};

////////////////////////////////////////////////////////////////////////////////////////
class CPlexDownloadFileJob : public CJob
{
  public:
    CPlexDownloadFileJob(const CStdString& url, const CStdString& destination) :
      CJob(), m_failed(false), m_url(url), m_destination(destination)
    {};

    virtual void Cancel() { m_http.Cancel(); }

    bool DoWork();
    CStdString m_url;
    CStdString m_destination;
    XFILE::CCurlFile m_http;

    bool m_failed;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
class CPlexThemeMusicPlayerJob : public CJob
{
  public:
    CPlexThemeMusicPlayerJob(const CStdString& themeUrl) : m_themeUrl(themeUrl) {}
    bool DoWork();
    CStdString m_themeUrl;
    CStdString m_fileToPlay;
};

#ifdef TARGET_RASPBERRY_PI
class CPlexAutoUpdate; 
class CPlexUpdaterJob : public CJob
{
  public:
    CPlexUpdaterJob(CPlexAutoUpdate *autoupdater, const CStdString& localbinary) : 
      m_localBinary(localbinary)
    {
      m_autoupdater = autoupdater;
    };

    bool DoWork();

    CPlexAutoUpdate *m_autoupdater;
    CStdString m_localBinary;
};
#endif

#endif /* defined(__Plex_Home_Theater__PlexJobs__) */
