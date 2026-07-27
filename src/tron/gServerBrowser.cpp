/*

*************************************************************************

ArmageTron -- Just another Tron Lightcycle Game in 3D.
Copyright (C) 2000  Manuel Moos (manuel@moosnet.de)

**************************************************************************

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
  
***************************************************************************

*/

#include "gServerBrowser.h"
#include "gGame.h"
#include "gLogo.h"
#include "gServerFavorites.h"
#include "gFriends.h"

#include "nServerInfo.h"
#include "nNetwork.h"

#include "ePlayer.h"

#include "rSysdep.h"
#include "rScreen.h"
#include "rConsole.h"
#include "rRender.h"

#include "uMenu.h"
#include "uInputQueue.h"

#include "tMemManager.h"
#include "tSysTime.h"
#include "tToDo.h"

#include "tDirectories.h"
#include "tConfiguration.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <cstdlib>
#include <sstream>
#include <string>

int gServerBrowser::lowPort  = 4534;

int gServerBrowser::highPort = 4540;
static bool continuePoll = false;
static int sg_simultaneous = 20;
static tSettingItem< int > sg_simultaneousConf( "BROWSER_QUERIES_SIMULTANEOUS", sg_simultaneous );

static tOutput *sg_StartHelpText = NULL;

nServerInfo::QueryType sg_queryType = nServerInfo::QUERY_OPTOUT;
tCONFIG_ENUM( nServerInfo::QueryType );
static tSettingItem< nServerInfo::QueryType > sg_query_type( "BROWSER_QUERY_FILTER", sg_queryType );

class gServerMenuItem;


class gServerInfo: public nServerInfo
{
public:
    gServerMenuItem *menuItem;
	bool show; //for server browser hiding
    bool webLobby;
    tString webLobbyCode;
    tString webLobbyRelayUrl;

    gServerInfo():menuItem(NULL), show(true), webLobby(false)
    {
    }

    virtual ~gServerInfo();

    void SetWebLobby( tString const & code, tString const & roomName, int playerCount, int maxPlayerCount,
                      tString const & relayUrl, tString const & relayHost, unsigned int relayPort )
    {
        webLobby = true;
        webLobbyCode = code;
        webLobbyRelayUrl = relayUrl;

        name = roomName;
        if ( name.Len() <= 1 )
        {
            name = "Wasmagetron Room ";
            name << code;
        }

        users = playerCount;
        maxUsers_ = maxPlayerCount;
        release_ = "Wasmagetron";
        userNames_ = userNamesOneLine_ = "Room code ";
        userNames_ << code;
        userNamesOneLine_ << code;
        options_ = "Hosted through the Wasmagetron relay.";
        url_ = relayUrl;
        ping = .001;
        score = 10000 + users;
        advancedInfoSet = true;
        advancedInfoSetEver = true;
        timesNotAnswered = 0;
        stillOnMasterServer = true;

        SetConnectionName( relayHost );
        SetPort( relayPort );
    }

    // during browsing, the whole server list consists of gServerInfos
    static gServerInfo * GetFirstServer()
    {
        return dynamic_cast< gServerInfo * >( nServerInfo::GetFirstServer() );
    }

    gServerInfo * Next()
    {
        return dynamic_cast< gServerInfo * >( nServerInfo::Next() );
    }
};

nServerInfo* CreateGServer()
{
    nServerInfo *ret = tNEW(gServerInfo);

    //if (!continuePoll)
    //{
    //    nServerInfo::StartQueryAll( sg_queryType );
    //    continuePoll = true;
    // }

    return ret;
}

#ifdef __EMSCRIPTEN__
static int sg_ToInt( std::string const & value, int fallback )
{
    std::istringstream stream( value );
    int parsed = fallback;
    stream >> parsed;
    return parsed;
}

// One room, as the page describes it: seven tab separated fields, code first
// and the relay's host and port last. Both the room list and hosting hand back
// rooms in this shape, so they share the parse.
static gServerInfo * sg_WebLobbyFromLine( std::string const & line )
{
    if ( line.empty() )
        return NULL;

    std::string fields[7];
    std::istringstream cols( line );
    for ( int i = 0; i < 7 && std::getline( cols, fields[i], '\t' ); ++i )
        ;

    if ( fields[0].empty() || fields[4].empty() || fields[5].empty() )
        return NULL;

    unsigned int relayPort = static_cast< unsigned int >( sg_ToInt( fields[6], 0 ) );
    if ( relayPort == 0 )
        return NULL;

    gServerInfo * server = dynamic_cast< gServerInfo * >( CreateGServer() );
    if ( !server )
        return NULL;

    server->SetWebLobby(
        tString( fields[0].c_str() ),
        tString( fields[1].c_str() ),
        sg_ToInt( fields[2], 0 ),
        sg_ToInt( fields[3], MAXCLIENTS ),
        tString( fields[4].c_str() ),
        tString( fields[5].c_str() ),
        relayPort
    );

    return server;
}

// Point emscripten's socket layer at this room's relay session. Every browser
// socket goes to one URL, so this has to be set before the connection is made
// and re-set for each room joined.
static void sg_UseRelay( tString const & relayUrl )
{
    EM_ASM({
        var relay = UTF8ToString($0);
        Module.websocket = Module.websocket || {};
        Module.websocket.url = relay;
        Module.websocket.subprotocol = 'binary';
        window.__aaRelayUrl = relay;
    }, static_cast< const char * >( relayUrl ) );
}

static void sg_AddWebLobbyServers()
{
    char * rooms = emscripten_run_script_string(
        "(function(){"
        "  return typeof window.__aaListRoomsForGame === 'function' ? window.__aaListRoomsForGame() : '';"
        "})()" );
    if ( !rooms || !*rooms )
    {
        if ( rooms )
            free( rooms );
        return;
    }

    std::istringstream lines( rooms );
    std::string line;
    while ( std::getline( lines, line ) )
        sg_WebLobbyFromLine( line );

    free( rooms );
}
#endif


class gServerMenu: public uMenu
{
    int sortKey_;

protected:
    virtual int ItemAt( REAL x, REAL y );

public:
    virtual void OnRender();

    void Update(); // sort the server view by score
    gServerMenu(const char *title);
    ~gServerMenu();

    virtual void HandleEvent( SDL_Event event );

    void Render(REAL y,
                const tString &servername, const tOutput &score,
                const tOutput &users     , const tOutput &ping);

    void Render(REAL y,
                const tString &servername, const tString &score,
                const tString &users     , const tString &ping);
};


class gBrowserMenuItem: public uMenuItem
{
protected:
    bool displayHelp_;
    REAL helpAlpha_;
    
    gBrowserMenuItem(uMenu *M,const tOutput &help): uMenuItem( M, help )
    {
    }

    // handles a key press
    virtual bool Event( SDL_Event& event );

    virtual void RenderBackground();

    virtual bool DisplayHelp( bool display, REAL y, REAL alpha )
    {
        helpAlpha_ = alpha;
        displayHelp_ = display;
        return false;
    }
};

class gServerMenuItem: public gBrowserMenuItem
{
protected:
    gServerInfo *server;
    double      lastPing_; //!< the time of the last manual ping
    bool        favorite_; //!< flag indicating whether this is a favorite
public:
    void AddFavorite();
    void SetServer(nServerInfo *s);
    gServerInfo *GetServer();

    virtual void Render(REAL x,REAL y,REAL alpha=1, bool selected=0);
    virtual void RenderBackground();

    virtual void Enter();

    // handles a key press
    virtual bool Event( SDL_Event& event );

    gServerMenuItem(gServerMenu *men);
    virtual ~gServerMenuItem();
};

class gServerStartMenuItem: public gBrowserMenuItem
{
public:
    virtual void Render(REAL x,REAL y,REAL alpha=1, bool selected=0);

    virtual void Enter();

    gServerStartMenuItem(gServerMenu *men);
    virtual ~gServerStartMenuItem();
};






static bool sg_RequestLANcontinuously = false;

void gServerBrowser::BrowseMaster()
{
    BrowseSpecialMaster(0,"");
}

// the currently active master
static nServerInfoBase * sg_currentMaster = 0;
nServerInfoBase * gServerBrowser::CurrentMaster()
{
    return sg_currentMaster;
}


void gServerBrowser::BrowseSpecialMaster( nServerInfoBase * master, char const * prefix )
{
    sg_currentMaster = master;

#ifdef __EMSCRIPTEN__
    (void)prefix; // no master server is consulted, so nothing filters on it
#endif

    sg_RequestLANcontinuously = false;

    sn_ServerInfoCreator *cback = nServerInfo::SetCreator(&CreateGServer);

    sr_con.autoDisplayAtNewline=true;
    sr_con.fullscreen=true;

#ifndef DEDICATED
    rSysDep::SwapGL();
    rSysDep::ClearGL();
    rSysDep::SwapGL();
    rSysDep::ClearGL();
#endif

    bool to=sr_textOut;
    sr_textOut=true;

    nServerInfo::DeleteAll();
#ifdef __EMSCRIPTEN__
    // The public master servers are unreachable from a browser: they speak UDP
    // to a resolved hostname, and neither the lookup nor the datagram survives
    // SOCKFS. GetFromMaster does not fail fast, though -- it tries each master
    // in the list in turn, and when the last one times out it puts up a message
    // box with a 3600 second timeout. So asking for the internet list used to
    // stall for the better part of a minute and then land on a modal error,
    // with the rooms that *are* reachable never getting drawn.
    //
    // Nothing on a master server could be joined from here anyway -- a browser
    // reaches a game only through a relay session -- so the control plane is
    // not a supplement to the master list here, it is the whole list.
    sg_AddWebLobbyServers();
#else
    nServerInfo::GetFromMaster( master, prefix );
    nServerInfo::Save();
#endif

    //  gLogo::SetBig(true);
    //  gLogo::SetSpinning(false);

    sr_textOut = to;

    tOutput StartHelpTextInternet("$network_master_host_inet_help");
    sg_StartHelpText = &StartHelpTextInternet;
    sg_TalkToMaster = true;

    BrowseServers();

    nServerInfo::Save();

    sg_TalkToMaster = false;

    nServerInfo::SetCreator(cback);

    sg_currentMaster = master;
}

void gServerBrowser::BrowseLAN()
{
    // TODO: reacivate and see what happens. Done.
    sg_RequestLANcontinuously = true;
    //	sg_RequestLANcontinuously = false;

    sn_ServerInfoCreator *cback = nServerInfo::SetCreator(&CreateGServer);

    sr_con.autoDisplayAtNewline=true;
    sr_con.fullscreen=true;

#ifndef DEDICATED
    rSysDep::SwapGL();
    rSysDep::ClearGL();
    rSysDep::SwapGL();
    rSysDep::ClearGL();
#endif

    bool to=sr_textOut;
    sr_textOut=true;

    nServerInfo::DeleteAll();
    nServerInfo::GetFromLAN(lowPort, highPort);

    sr_textOut = to;

    tOutput StartHelpTextLAN("$network_master_host_lan_help");
    sg_StartHelpText = &StartHelpTextLAN;
    sg_TalkToMaster = false;

    BrowseServers();

    nServerInfo::SetCreator(cback);
}

void gServerBrowser::BrowseServers()
{
    //nServerInfo::CalcScoreAll();
    //nServerInfo::Sort();
#ifndef __EMSCRIPTEN__
    nServerInfo::StartQueryAll( sg_queryType );
    continuePoll = true;
#else
    // Every entry here is a lobby room, and a room cannot be polled: the query
    // is a UDP round trip to the server's own address, and the only address a
    // room has is a relay endpoint that speaks the game protocol over one
    // WebSocket. Querying them means opening sockets that never answer, which
    // is the browser locking up rather than a slow list.
    //
    // Nothing is lost. The listing already carries everything a query would
    // fetch -- name, player count, capacity -- so SetWebLobby marks the info
    // complete and there is nothing left to ask for.
#endif

    gServerMenu browser("Server Browser");

    gServerStartMenuItem start(&browser);

    /*
      while (nServerInfo::DoQueryAll(sg_simultaneous));
      sn_SetNetState(nSTANDALONE);
      nServerInfo::Sort();

      if (nServerInfo::GetFirstServer())
      ConnectToServer(nServerInfo::GetFirstServer());
    */
    browser.Update();

    // eat excess input the user made while the list was fetched
    SDL_Event ignore;
    REAL time;
    while(su_GetSDLInput(ignore, time)) ;

    browser.Enter();

    nServerInfo::GetFromLANContinuouslyStop();

    //  gLogo::SetBig(false);
    //  gLogo::SetSpinning(true);
    // gLogo::SetDisplayed(true);
}





void gServerMenu::HandleEvent( SDL_Event event )
{
#ifndef DEDICATED
    switch (event.type)
    {
    case SDL_KEYDOWN:
        switch (event.key.keysym.sym)
        {
        case(SDLK_LEFT):
            sortKey_ = ( sortKey_ + nServerInfo::KEY_MAX-1 ) % nServerInfo::KEY_MAX;
            Update();
            return;
            break;
        case(SDLK_RIGHT):
            sortKey_ = ( sortKey_ + 1 ) % nServerInfo::KEY_MAX;
            Update();
            return;
            break;
		case(SDLK_m):
			FriendsToggle();
            Update();
			return;
			break;
        default:
            break;
        }
    }
#endif

    uMenu::HandleEvent( event );
}

void gServerMenu::OnRender()
{
    uMenu::OnRender();

    // next time the server list is to be resorted
    static double sg_serverMenuRefreshTimeout=-1E+32f;

    if (sg_serverMenuRefreshTimeout < tSysTimeFloat())
    {
        Update();
        sg_serverMenuRefreshTimeout = tSysTimeFloat()+2.0f;
    }
}

void gServerMenu::Update()
{
    // get currently selected server
    gServerMenuItem *item = NULL;
    if ( selected < items.Len() )
    {
        item = dynamic_cast<gServerMenuItem*>(items(selected));
    }
    gServerInfo* info = NULL;
    if ( item )
    {
        info = item->GetServer();
    }

    // keep the cursor position relative to the top, if possible
    int selectedFromTop = items.Len() - selected;

    ReverseItems();

    nServerInfo::CalcScoreAll();
    nServerInfo::Sort( nServerInfo::PrimaryKey( sortKey_ ) );

    int mi = 1;
    gServerInfo *run = gServerInfo::GetFirstServer();
	bool oneFound = false; //so we can display all if none were found
    while (run)
    {
		//check friend filter
		if (getFriendsEnabled())
		{
			run->show = false;
			int i;
			tString userNames = run->UserNames();
			tString* friends = getFriends();
			for (i = MAX_FRIENDS-1; i>=0; i--)
			{
				if (run->Users() > 0 && friends[i].Len() > 1 && userNames.StrPos(friends[i]) >= 0)
				{
					oneFound = true;
					run->show = true;
				}
			}
		}
        run = run->Next();
	}

	run = gServerInfo::GetFirstServer();
	{
   		while (run)
    	{
			if (run->show || oneFound == false)
			{
	        	if (mi >= items.Len())
    		        tNEW(gServerMenuItem)(this);

    	    	gServerMenuItem *item = dynamic_cast<gServerMenuItem*>(items(mi));
    	    	item->SetServer(run);
	    	    mi++;
			}
        	run = run->Next();
		}
    }

    if (items.Len() == 1)
        selected = 1;

    while(mi < items.Len() && items.Len() > 2)
    {
        uMenuItem *it = items(items.Len()-1);
        delete it;
    }

    ReverseItems();

    // keep the cursor position relative to the top, if possible ( calling function will handle the clamping )
    selected = items.Len() - selectedFromTop;

    // set cursor to currently selected server, if possible
    if ( info && info->menuItem )
    {
        selected = info->menuItem->GetID();
    }

    if (sg_RequestLANcontinuously)
    {
        static REAL timeout=-1E+32f;

        if (timeout < tSysTimeFloat())
        {
            nServerInfo::GetFromLANContinuously();
            if (!continuePoll)
            {
                nServerInfo::StartQueryAll( sg_queryType );
                continuePoll = true;
            }
            timeout = tSysTimeFloat()+10;
        }
    }
}

gServerMenu::gServerMenu(const char *title)
        : uMenu(title, false)
        , sortKey_( nServerInfo::KEY_SCORE )
{
    nServerInfo *run = nServerInfo::GetFirstServer();
    while (run)
    {
        gServerMenuItem *item = tNEW(gServerMenuItem)(this);
        item->SetServer(run);
        run = run->Next();
    }

    ReverseItems();

    if (items.Len() <= 0)
    {
        selected = 1;
        tNEW(gServerMenuItem)(this);
    }
    else
        selected = items.Len();
}

gServerMenu::~gServerMenu()
{
    for (int i=items.Len()-1; i>=0; i--)
        delete items(i);
}

#ifndef DEDICATED
static REAL text_height=.05;
static REAL text_width=.025;

static REAL shrink = .6f;
static REAL displace = .15;

int gServerMenu::ItemAt( REAL x, REAL y )
{
    return uMenu::ItemAt( x, ( y - displace ) / shrink );
}

void gServerMenu::Render(REAL y,
                         const tString &servername, const tString &score,
                         const tString &users     , const tString &ping)
{
    if (sr_glOut)
    {
        REAL const xAspectMultiplier = rTextField::AspectWidthMultiplier();
        rTextField c(-.9f * xAspectMultiplier, y+text_height*.5, text_width * xAspectMultiplier, text_height);
        c.SetWidth(1000);

        c.SetIndent(5);


        //int posDisplacement = 0;
        tColoredString text;
        if (tColoredString::RemoveColors(servername).Len() > 1)
        {
            text << servername << "0xRESETT";
        }
        else
            text << tOutput("$network_master_unknown");

        if (ping.Len() > 1)
        {
            text.SetPos(static_cast<int>(1.35*xAspectMultiplier/c.GetCWidth())  - tColoredString::RemoveColors( ping ).Len() - 1, true );
            text << "0xRESETT";
            text << " " << ping;
        }
        else
        {
            text << "0xRESETT";
        }

        if (users.Len() > 1)
        {
            text.SetPos(static_cast<int>(1.6*xAspectMultiplier/c.GetCWidth()) - tColoredString::RemoveColors( users ).Len(), false );
            text << users;
        }

        if (score.Len() > 1)
        {
            text.SetPos(static_cast<int>(1.8*xAspectMultiplier/c.GetCWidth()) - tColoredString::RemoveColors( score ).Len(), false );
            text << score;
        }

        c << text;
    }
}

void gServerMenu::Render(REAL y,
                         const tString &servername, const tOutput &score,
                         const tOutput &users     , const tOutput &ping)
{
    tColoredString highlight, normal;
    highlight << tColoredString::ColorString( 1,.7,.7 );
    normal << tColoredString::ColorString( .7,.3,.3 );

    tString sn, s, u, p;

    sn << normal;
    s << normal;
    u << normal;
    p << normal;

    switch ( sortKey_ )
    {
    case nServerInfo::KEY_NAME:
        sn = highlight;
        break;
    case nServerInfo::KEY_PING:
        p = highlight;
        break;
    case nServerInfo::KEY_USERS:
        u = highlight;
        break;
    case nServerInfo::KEY_SCORE:
        s = highlight;
        break;
    case nServerInfo::KEY_MAX:
        break;
    }

    sn << servername;// tColoredString::RemoveColors( servername );
    s  << score;
    u  << users;
    p  << ping;

    Render(y, sn, s, u, p);
}

#endif /* DEDICATED */
static bool sg_filterServernameColorStrings = true;
static tSettingItem< bool > removeServerNameColors("FILTER_COLOR_SERVER_NAMES", sg_filterServernameColorStrings);
static bool sg_filterServernameDarkColorStrings = true;
static tSettingItem< bool > removeServerNameDarkColors("FILTER_DARK_COLOR_SERVER_NAMES", sg_filterServernameDarkColorStrings);

void gServerMenuItem::Render(REAL x,REAL y,REAL alpha, bool selected)
{
#ifndef DEDICATED
    // REAL time=tSysTimeFloat()*10;

    SetColor( selected, alpha );

    gServerMenu *serverMenu = static_cast<gServerMenu*>(menu);

    if (server)
    {
        tColoredString name;
        tString score;
        tString users;
        tString ping;

        int p = static_cast<int>(server->Ping()*1000);
        if (p < 0)
            p = 0;
        if (p > 10000)
            p = 10000;

        int s = static_cast<int>(server->Score());
        if (server->Score() > 10000)
            s = 10000;
        if (server->Score() < -10000)
            s = -10000;

        if (server->Polling())
        {
            score << tOutput("$network_master_polling");
        }
        else if (!server->Reachable())
        {
            score << tOutput("$network_master_unreachable");
        }
        else if ( nServerInfo::Compat_Ok != server->Compatibility() )
        {
            switch( server->Compatibility() )
            {
            case nServerInfo::Compat_Upgrade:
                score << tOutput( "$network_master_upgrage" );
                break;
            case nServerInfo::Compat_Downgrade:
                score << tOutput( "$network_master_downgrage" );
                break;
            default:
                score << tOutput( "$network_master_incompatible" );
                break;
            }
        }
        else if ( server->Users() >= server->MaxUsers() )
        {
            score << tOutput( "$network_master_full" );
            score << " (" << server->Users() << "/" << server->MaxUsers() << ")";
        }
        else
        {
            if ( favorite_ )
            {
                score << "B ";
            }

            score << s;
            users << server->Users() << "/" << server->MaxUsers();
            ping  << p;
        }

        if ( sg_filterServernameColorStrings )
            name << tColoredString::RemoveColors( server->GetName(), false );
	else if ( sg_filterServernameDarkColorStrings )
            name << tColoredString::RemoveColors( server->GetName(), true );
        else
        {
            name << server->GetName();
        }

        serverMenu->Render(y*shrink + displace,
                           name,
                           score, users, ping);
    }
    else
    {
        tOutput o("$network_master_noserver");
        tString s;
        s << o;
        serverMenu->Render(y*shrink + displace,
                           s,
                           tString(""), tString(""), tString(""));

    }
#endif
}

static REAL sg_menuBottom    = -.9;
static REAL sg_requestBottom = -.9;

void gServerMenuItem::RenderBackground()
{
#ifndef DEDICATED
    REAL helpTopReal = sg_requestBottom*shrink + displace - .05;;

    gBrowserMenuItem::RenderBackground();

    rTextField::SetDefaultColor( tColor(1,1,1) );

    REAL const xAspectMultiplier = rTextField::AspectWidthMultiplier();
    rTextField players( -.9, helpTopReal, text_width * xAspectMultiplier, text_height );
    if ( server )
    {
        players << tOutput( "$network_master_players" );
        if ( server->UserNamesOneLine().Len() > 2 )
            players << server->UserNamesOneLine();
        else
            players << tOutput( "$network_master_players_empty" );
        players << "\n" << tColoredString::ColorString(1,1,1);
        tColoredString uri;
        uri << server->Url() << tColoredString::ColorString(1,1,1);
        players << tOutput( "$network_master_serverinfo", server->Release(), uri, server->Options() );
    }

    if( displayHelp_ )
    {
        players << "\n";
        players.SetColor(tColor(1,1,1,helpAlpha_));
        players << Help();
    }

    REAL helpSpace = players.GetTop() - players.GetBottom();
    REAL helpTop = -.85 + helpSpace;
    REAL helpTopScaled = ( helpTop - displace )/shrink;
    REAL helpTopMax = .25;
    REAL helpTopMin = -.9;
    if( helpTopScaled > helpTopMax )
    {
        helpTopScaled = helpTopMax;
    }
    if( helpTopScaled < helpTopMin )
    {
        helpTopScaled = helpTopMin;
    }
    sg_requestBottom = helpTopScaled;
#endif
}

#ifndef DEDICATED
static void Refresh()
{
    continuePoll = true;
    nServerInfo::StartQueryAll( sg_queryType );
}
#endif

bool gBrowserMenuItem::Event( SDL_Event& event )
{
#ifndef DEDICATED
    switch (event.type)
    {
    case SDL_KEYDOWN:
        switch (event.key.keysym.sym)
        {
        case SDLK_r:
            {
                static double lastRefresh = - 100; //!< the time of the last manual refresh
                if ( tSysTimeFloat() - lastRefresh > 2.0 )
                {
                    lastRefresh = tSysTimeFloat();
                    // trigger refresh
                    st_ToDo( Refresh );
                    return true;
                }
            }
            break;
        default:
            break;
        }
    }
#endif

    return uMenuItem::Event( event );
}


bool gServerMenuItem::Event( SDL_Event& event )
{
#ifndef DEDICATED
    switch (event.type)
    {
    case SDL_KEYDOWN:
        switch (event.key.keysym.sym)
        {
        case SDLK_p:
            continuePoll = true;
            if ( server && tSysTimeFloat() - lastPing_ > .5f )
            {
                lastPing_ = tSysTimeFloat();

                server->SetQueryType( nServerInfo::QUERY_ALL );
                server->QueryServer();
                server->ClearInfoFlags();
            }
            return true;
            break;
        default:
            break;
        }
        switch (event.key.keysym.unicode)
        {
        case '+':
            if ( server )
            {
                server->SetScoreBias( server->GetScoreBias() + 10 );
                server->CalcScore();
            }
            (static_cast<gServerMenu*>(menu))->Update();

            return true;
            break;
        case '-':
            if ( server )
            {
                server->SetScoreBias( server->GetScoreBias() - 10 );
                server->CalcScore();
            }
            (static_cast<gServerMenu*>(menu))->Update();

            return true;
            break;
        case 'b':
            if ( server && !favorite_ )
            {
                favorite_ = gServerFavorites::AddFavorite( server );
            }
            return true;
            break;
        default:
            break;
        }
    }
#endif

    return gBrowserMenuItem::Event( event );
}

void gBrowserMenuItem::RenderBackground()
{
    if( menu )
    {
        double now = tSysTimeFloat();
        static double lastTime = now;
        if( sg_menuBottom > sg_requestBottom )
        {
            sg_menuBottom -= now - lastTime;
        }
        lastTime = now;
        if( sg_menuBottom < sg_requestBottom )
        {
            sg_menuBottom = sg_requestBottom;
        }

        menu->SetBot( sg_menuBottom );
        sg_requestBottom = -.9;
    }

    sn_Receive();
    sn_SendPlanned();

    menu->GenericBackground();
    if (continuePoll)
    {
        continuePoll = nServerInfo::DoQueryAll(sg_simultaneous);
        sn_Receive();
        sn_SendPlanned();
    }

#ifndef DEDICATED
    rTextField::SetDefaultColor( tColor(.8,.3,.3,1) );

	tString sn2 = tString(tOutput("$network_master_servername"));
	if (getFriendsEnabled()) //display that friends filter is on
		sn2 << " - " << tOutput("$friends_enable");

    static_cast<gServerMenu*>(menu)->Render(.62,
                                            sn2,
                                            tOutput("$network_master_score"),
                                            tOutput("$network_master_users"),
                                            tOutput("$network_master_ping"));
#endif
}

void gServerMenuItem::Enter()
{
    nServerInfo::GetFromLANContinuouslyStop();

    menu->Exit();

    //  gLogo::SetBig(false);
    //  gLogo::SetSpinning(true);
    // gLogo::SetDisplayed(false);

#ifdef __EMSCRIPTEN__
    if ( server && server->webLobby )
    {
        sg_UseRelay( server->webLobbyRelayUrl );
    }
#endif

    if (server)
        ConnectToServer(server);
}

#ifdef __EMSCRIPTEN__
// Host a match and watch it from here.
//
// Hosting cannot mean natively what it means here. sn_SetNetState(nSERVER)
// makes this tab the game server, but nothing can ever reach it: a browser has
// no listening socket, and a relay session is always opened from the browser
// side. The room code a host hands out resolves to the dedicated server behind
// the relay, so a browser that made itself the server and the phones that
// joined its code were in two different games -- which is why a hosted room
// could look created, joinable and completely empty at the same time.
//
// So a hosted match is one the host joins too. This tab becomes a client like
// any other; it just joins as a spectator with the camera pulled back, which is
// what turns a laptop on a TV into the shared view of the match while everyone
// else plays from their phones.
extern bool sg_hostAsSpectator;

void gServerBrowser::HostBigScreenMatch()
{
    // Handed over as a global rather than interpolated into the script, so a
    // server name with a quote in it cannot become JavaScript.
    EM_ASM({
        window.__aaPendingRoomName = UTF8ToString($0);
    }, static_cast< const char * >( sn_serverName ) );

    char * room = emscripten_run_script_string(
        "(function(){"
        "  return typeof window.__aaCreateHostedRoom === 'function'"
        "    ? window.__aaCreateHostedRoom(window.__aaPendingRoomName) : '';"
        "})()" );

    std::string line( room ? room : "" );
    if ( room )
        free( room );

    nServerInfo::DeleteAll();
    gServerInfo * server = sg_WebLobbyFromLine( line );
    if ( !server )
    {
        tConsole::Message( "$network_host_failed_title", "$network_host_failed_inter", 10 );
        return;
    }

    // Spectating is opt in. A host who only watches leaves the room with no
    // player in it, and the server will not start a match for an empty grid --
    // it sits in "waiting for real players" until someone else arrives. That is
    // the right behaviour for a big screen someone is about to join, and a
    // bafflingly dead one for anybody who just wanted to host a game.
    //
    // The settings below belong to the player and outlive the match, so they
    // are put back afterwards. Otherwise hosting once as a spectator would
    // silently make you one in every game you joined after it.
    ePlayer * lp = sg_hostAsSpectator ? ePlayer::PlayerConfig( 0 ) : NULL;
    bool     wasSpectating = lp ? lp->spectate : false;
    eCamMode wasCamera     = lp ? lp->startCamera : CAMERA_SMART;
    bool     wasFreeCam    = lp ? lp->allowCam[ CAMERA_FREE ] : false;

    if ( lp )
    {
        lp->spectate = true;
        // The free camera is the only one that is not welded to a cycle, and a
        // spectator has no cycle to weld to. Started high and behind the middle
        // of the arena, it frames the whole grid rather than one player.
        lp->startCamera = CAMERA_FREE;
        lp->allowCam[ CAMERA_FREE ] = true;

        std::stringstream cameraSettings(
            "CAMERA_FREE_START_X 0\n"
            "CAMERA_FREE_START_Y -60\n"
            "CAMERA_FREE_START_Z 200\n" );
        tConfItemBase::LoadAll( cameraSettings );
    }

    sg_UseRelay( server->webLobbyRelayUrl );
    ConnectToServer( server );

    if ( lp )
    {
        lp->spectate = wasSpectating;
        lp->startCamera = wasCamera;
        lp->allowCam[ CAMERA_FREE ] = wasFreeCam;
    }

    EM_ASM({
        if (typeof window.__aaCloseHostedRoom === 'function') window.__aaCloseHostedRoom();
    });
}
#endif


void gServerMenuItem::SetServer(nServerInfo *s)
{
    if (s == server)
        return;

    if (server)
        server->menuItem = NULL;

    server = dynamic_cast<gServerInfo*>(s);

    if (server)
    {
        if (server->menuItem)
            server->menuItem->SetServer(NULL);

        server->menuItem = this;
    }

    favorite_ = gServerFavorites::IsFavorite( server );
}

gServerInfo *gServerMenuItem::GetServer()
{
    return server;
}

static char const * sg_HelpText = "$network_master_browserhelp";

gServerMenuItem::gServerMenuItem(gServerMenu *men)
        :gBrowserMenuItem(men, sg_HelpText), server(NULL), lastPing_(-100), favorite_(false)
{}

gServerMenuItem::~gServerMenuItem()
{
    SetServer(NULL);

    // make sure the last entry in the array (the first menuitem)
    // stays the same
    uMenuItem* last = menu->Item(menu->NumItems()-1);
    menu->RemoveItem(last);
    menu->RemoveItem(this);
    menu->AddItem(last);
}


gServerInfo::~gServerInfo()
{
    if (menuItem)
        delete menuItem;
}


void gServerStartMenuItem::Render(REAL x,REAL y,REAL alpha, bool selected)
{
#ifndef DEDICATED
    // REAL time=tSysTimeFloat()*10;

    SetColor( selected, alpha );

    tString s;
    s << tOutput("$network_master_start");
    static_cast<gServerMenu*>(menu)->Render(y*shrink + displace,
                                            s,
                                            tString(), tString(), tString());
#endif
}

void gServerStartMenuItem::Enter()
{
    nServerInfo::GetFromLANContinuouslyStop();

    menu->Exit();

    //  gLogo::SetBig(false);
    //  gLogo::SetSpinning(true);
    // gLogo::SetDisplayed(false);

    sg_HostGameMenu();
}



gServerStartMenuItem::gServerStartMenuItem(gServerMenu *men)
        :gBrowserMenuItem(men, *sg_StartHelpText)
{}

gServerStartMenuItem::~gServerStartMenuItem()
{
}


