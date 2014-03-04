

#include "Owned/PlexGlobalCacher.h"
#include "FileSystem/PlexDirectory.h"
#include "Client/PlexServerDataLoader.h"
#include "PlexApplication.h"
#include "utils/Stopwatch.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "guilib/GUITextBox.h"
#include "guilib/GUIEditControl.h"
#include "dialogs/GUIDialogYesNo.h"
#include "utils/log.h"
#include "filesystem/File.h"
#include "TextureCache.h"
#include "guilib/GUIWindowManager.h"

using namespace XFILE;

#define GLOBAL_CACHING_DESC "Precaching Metadata"

CPlexGlobalCacher* CPlexGlobalCacher::m_globalCacher = NULL;

CPlexGlobalCacher::CPlexGlobalCacher() : CPlexThumbCacher() , CThread("Plex Global Cacher")
{
  m_continue = true;

  m_dlgProgress = (CGUIDialogProgress*)g_windowManager.GetWindow(WINDOW_DIALOG_PROGRESS);
  if (m_dlgProgress)
  {
    m_dlgProgress->SetHeading(2);
    m_dlgProgress->SetLine(0, "Line 0");
    m_dlgProgress->SetLine(1, "Line 1");
    m_dlgProgress->SetLine(2, "Line 2");
  }

}

CPlexGlobalCacher::~CPlexGlobalCacher()
{
}


CPlexGlobalCacher* CPlexGlobalCacher::getGlobalCacher()
{
  if (!m_globalCacher)   // Only allow one instance of class to be generated.
    m_globalCacher = new CPlexGlobalCacher();
  return m_globalCacher;
}

void CPlexGlobalCacher::Continue(bool cont)
{
  m_continue = cont;
}

void CPlexGlobalCacher::Start()
{
  CLog::Log(LOGNOTICE,"Global Cache : Creating cacher thread");
  CThread::Create(true);
  CLog::Log(LOGNOTICE,"Global Cache : Cacher thread created");
}


void controlGlobalCache()
{
  CPlexGlobalCacher* cacher = CPlexGlobalCacher::getGlobalCacher();


  //virtual void StopThread(bool bWait = true);
  // bool IsRunning() const;

  if ( ! cacher->IsRunning() )
  {
    bool ok = CGUIDialogYesNo::ShowAndGetInput("Start global caching?",
                                               "This will cache all metadata and thumbnails automatically.",
                                               "Larger libraries will take longer to precache.",
                                               "Reboot for best performance once caching is complete.",
                                               "No!", "Yes");

    if ( ok )
    {
      cacher->Start();
      cacher->Continue(true);
    }
  }
  else if ( cacher ->IsRunning() )
  {
    bool ok = CGUIDialogYesNo::ShowAndGetInput("Stop global caching?",
                                               "A reboot is recommended after stopping ",
                                               "the global caching process.",
                                               "Stopping may not occur right away.",
                                               "No!", "Yes");

    if ( ok )
    {
      CLog::Log(LOGNOTICE,"Global Cache : Cacher thread stopped by user");
      cacher -> StopThread(false);
      cacher -> Continue(false);
    }
  }
}



void CPlexGlobalCacher::Process()
{
  CPlexDirectory dir;
  CFileItemList list;
  CStopWatch timer;
  CStopWatch looptimer;
  CStopWatch msgtimer;
  CStdString message;
  int totalsize =0;

  m_dlgProgress->SetHeading("Precaching");
  m_dlgProgress->StartModal();
  m_dlgProgress->ShowProgressBar(true);

  CFileItemListPtr pAllSharedSections = g_plexApplication.dataLoader->GetAllSharedSections();
  CLog::Log(LOGNOTICE,"Global Cache : found %d Shared Sections",pAllSharedSections->Size());

  // get all the sections names
  CFileItemListPtr pAllSections = g_plexApplication.dataLoader->GetAllSections();
  pAllSections->Append(*pAllSharedSections);

  CLog::Log(LOGNOTICE,"Global Cache : found %d Regular Sections",pAllSections->Size());

  timer.StartZero();
  msgtimer.StartZero();
  for (int iSection = 0; iSection < pAllSections->Size() && m_continue; iSection ++)
  {

    list.Clear();

    looptimer.StartZero();
    CFileItemPtr Section = pAllSections->Get(iSection);

    message.Format("Precaching Section %d of %d : '%s'",iSection+1,pAllSections->Size(),Section->GetLabel());
    m_dlgProgress->SetLine(0,message);

    // Pop the notification
    message.Format("Action : Retrieving content from '%s'...", Section->GetLabel());
    m_dlgProgress->SetLine(1,message);
    m_dlgProgress->SetLine(2,"");
    m_dlgProgress->SetPercentage(0);

    // gets all the data from one section
    CURL url(Section->GetPath());
    PlexUtils::AppendPathToURL(url, "all");

    // store them into the list
    dir.GetDirectory(url, list);

    // Grab the server Name
    CStdString ServerName = "<unknown>";
    if (list.Size())
    {
      CPlexServerPtr  pServer = g_plexApplication.serverManager->FindFromItem(list.Get(0));
      if (pServer)
      {
        ServerName = pServer->GetName();
      }

      CLog::Log(LOGNOTICE,"Global Cache : Processed +%d items in '%s' on %s (total %d), took %f (total %f)", list.Size(), Section->GetLabel().c_str(), ServerName.c_str(), totalsize, looptimer.GetElapsedSeconds(),timer.GetElapsedSeconds());
    }

    totalsize += list.Size();

    // Here we have the file list, just process the items
    for (int i = 0; i < list.Size() && m_continue; i++)
    {
      CFileItemPtr item = list.Get(i);
      if (item->IsPlexMediaServer())
      {
        message.Format("Precaching Section %d of %d : '%s' on '%s' ", iSection+1, pAllSections->Size(), Section->GetLabel(), ServerName);
        m_dlgProgress->SetLine(0,message);

        m_dlgProgress->SetPercentage(i*100 / list.Size());

        message.Format("Action : Precaching item %d/%d ...", i, list.Size());
        m_dlgProgress->SetLine(1,message);

        message.Format("Progress : %2d%%",i*100 / list.Size());
        m_dlgProgress->SetLine(2,message);

        CStdStringArray art;
        art.push_back("smallThumb");
        art.push_back("smallPoster");
        art.push_back("smallGrandparentThumb");

        BOOST_FOREACH(CStdString artKey, art)
        {
          if (item->HasArt(artKey) &&
              !CTextureCache::Get().HasCachedImage(item->GetArt(artKey)))
            CTextureCache::Get().CacheImage(item->GetArt(artKey));
        }

        // check if cancel button has been pressed
        if (m_dlgProgress->IsCanceled())
        {
          CLog::Log(LOGNOTICE,"Global Cache : Operation was canceled by user");
          break;
        }
      }

      m_dlgProgress->Progress();
    }

    CLog::Log(LOGNOTICE,"Global Cache : Processing section %s took %f",Section->GetLabel().c_str(), timer.GetElapsedSeconds());
  }

  CLog::Log(LOGNOTICE,"Global Cache : Full operation took %f",timer.GetElapsedSeconds());
}


void CPlexGlobalCacher::OnExit()
{

  m_dlgProgress->Close();
  m_globalCacher = NULL;

}
