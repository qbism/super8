    #include <windows.h>
    #include <wininet.h>
    #include <urlmon.h>


    typedef int (*DOWNLOADPROGRESSPROC) (double);

    class CDownloader : public IBindStatusCallback
    {
    public:
       CDownloader (DOWNLOADPROGRESSPROC progressproc, DWORD maxtime, DWORD timeout)
       {
          // progress updater (optional)
          this->DownloadProgressProc = progressproc;

          // store times in milliseconds for convenience
          this->DownloadMaxTime = maxtime * 1000;
          this->DownloadTimeOut = timeout * 1000;

          // init to sensible values
          this->DownloadBeginTime = timeGetTime ();
          this->DownloadCurrentTime = timeGetTime ();
          this->DownloadLastCurrentTime = timeGetTime ();
       }

       ~CDownloader (void) {}

       // progress
       STDMETHOD (OnProgress) (ULONG ulProgress, ULONG ulProgressMax, ULONG ulStatusCode, LPCWSTR szStatusText)
       {
          switch (ulStatusCode)
          {
          case BINDSTATUS_BEGINDOWNLOADDATA:
             // init timers
             this->DownloadBeginTime = timeGetTime ();
             this->DownloadCurrentTime = timeGetTime ();
             this->DownloadLastCurrentTime = timeGetTime ();
             return S_OK;

          case BINDSTATUS_ENDDOWNLOADDATA:
             // amount downloaded was not equal to the amount needed
             if (ulProgress != ulProgressMax) return E_ABORT;

             // alles gut
             return S_OK;

          case BINDSTATUS_DOWNLOADINGDATA:
             // update current time
             this->DownloadCurrentTime = timeGetTime ();

             // abort if the max download time is exceeded
             if (this->DownloadCurrentTime - this->DownloadBeginTime > this->DownloadMaxTime) return E_ABORT;

             // check for a hang and abort if required
             if (this->DownloadCurrentTime - this->DownloadLastCurrentTime > this->DownloadTimeOut) return E_ABORT;

             // update hang check
             this->DownloadLastCurrentTime = this->DownloadCurrentTime;

             // send through the standard proc
             if (ulProgressMax && this->DownloadProgressProc)
                return (this->DownloadProgressProc ((int) ((((double) ulProgress / (double) ulProgressMax) * 100) + 0.5)) ? S_OK : E_ABORT);
             else return S_OK;

          default:
             break;
          }

          return S_OK;
       }

       // unimplemented methods
       STDMETHOD (GetBindInfo) (DWORD *grfBINDF, BINDINFO *pbindinfo) {return E_NOTIMPL;}
       STDMETHOD (GetPriority) (LONG *pnPriority) {return E_NOTIMPL;}
       STDMETHOD (OnDataAvailable) (DWORD grfBSCF, DWORD dwSize, FORMATETC *pformatetc, STGMEDIUM *pstgmed ) {return E_NOTIMPL;}
       STDMETHOD (OnObjectAvailable) (REFIID riid, IUnknown *punk) {return E_NOTIMPL;}
       STDMETHOD (OnStartBinding) (DWORD dwReserved, IBinding *pib) {return E_NOTIMPL;}
       STDMETHOD (OnStopBinding) (HRESULT hresult, LPCWSTR szError){return E_NOTIMPL;}
       STDMETHOD (OnLowResource) (DWORD blah) {return E_NOTIMPL;}

       // IUnknown methods - URLDownloadToFile never calls these
       STDMETHOD_ (ULONG, AddRef) () {return 0;}
       STDMETHOD_ (ULONG, Release) () {return 0;}
       STDMETHOD (QueryInterface) (REFIID riid, void __RPC_FAR *__RPC_FAR *ppvObject) {return E_NOTIMPL;}

    private:
       DOWNLOADPROGRESSPROC DownloadProgressProc;
       DWORD DownloadBeginTime;
       DWORD DownloadCurrentTime;
       DWORD DownloadLastCurrentTime;
       DWORD DownloadMaxTime;
       DWORD DownloadTimeOut;
    };


    typedef HRESULT (__stdcall *URLDOWNLOADTOFILEPROC) (LPUNKNOWN, LPCSTR, LPCSTR, DWORD, LPBINDSTATUSCALLBACK);
    typedef BOOL (__stdcall *DELETEURLCACHEENTRYPROC) (LPCSTR);

    extern "C" int Web_Get (const char *url, const char *referer, const char *name, int resume, int max_downloading_time, int timeout, int (*_progress) (double))
    {
       // assume it's failed until we know it's succeeded
       int DownloadResult = 0;

       // this is to deal with kiddies who still think it's cool to remove all IE components from their computers,
       // and also to resolve potential issues when we might be linking to a different version than what's on the
       // target machine.  in theory we could just use standard URLDownloadToFile and DeleteUrlCacheEntry though.
       HINSTANCE hInstURLMON = LoadLibrary ("urlmon.dll");
       HINSTANCE hInstWININET = LoadLibrary ("wininet.dll");

       if (hInstURLMON && hInstWININET)
       {
          // grab the non-unicode versions of the functions
          URLDOWNLOADTOFILEPROC QURLDownloadToFile = (URLDOWNLOADTOFILEPROC) GetProcAddress (hInstURLMON, "URLDownloadToFileA");
          DELETEURLCACHEENTRYPROC QDeleteUrlCacheEntry = (DELETEURLCACHEENTRYPROC) GetProcAddress (hInstWININET, "DeleteUrlCacheEntryA");

          if (QURLDownloadToFile && QDeleteUrlCacheEntry)
          {
             // always take a fresh copy (fail silently if this fails)
             QDeleteUrlCacheEntry (url);

             // create the COM interface used for downloading
             CDownloader *DownloadClass = new CDownloader (_progress, max_downloading_time, timeout);

             // and download it
             HRESULT hrDownload = QURLDownloadToFile (NULL, url, name, 0, DownloadClass);

             // clean up
             delete DownloadClass;

             // check result
             if (FAILED (hrDownload))
                DownloadResult = 0;
             else DownloadResult = 1;
          }
          else DownloadResult = 0;
       }
       else DownloadResult = 0;

       if (hInstURLMON) FreeLibrary (hInstURLMON);
       if (hInstWININET) FreeLibrary (hInstWININET);

       return DownloadResult;
    }
