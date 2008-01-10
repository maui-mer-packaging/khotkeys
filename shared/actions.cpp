/****************************************************************************

 KHotKeys

 Copyright (C) 1999-2001 Lubos Lunak <l.lunak@kde.org>

 Distributed under the terms of the GNU General Public License version 2.

****************************************************************************/

#define _ACTIONS_CPP_

#include "actions.h"

#include <krun.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kdebug.h>
#include <kurifilter.h>
#include <kglobal.h>
#include <kstandarddirs.h>
#include <kapplication.h>
#include <kdesktopfile.h>
#include <kworkspace.h>
#include <klocale.h>
#include <kservice.h>
#include <kprocess.h>

#include "windows.h"
#include "action_data.h"

#include <X11/X.h>
#include <QX11Info>
//Added by qt3to4:
#include <Q3PtrList>
#include <kauthorized.h>

namespace KHotKeys
{

// Action

Action* Action::create_cfg_read( KConfigGroup& cfg_P, Action_data* data_P )
    {
    QString type = cfg_P.readEntry( "Type" );
    if( type == "COMMAND_URL" )
        return new Command_url_action( cfg_P, data_P );
    if( type == "MENUENTRY" )
        return new Menuentry_action( cfg_P, data_P );
    if( type == "KEYBOARD_INPUT" )
        return new Keyboard_input_action( cfg_P, data_P );
    if( type == "ACTIVATE_WINDOW" )
        return new Activate_window_action( cfg_P, data_P );
    kWarning( 1217 ) << "Unknown Action type read from cfg file\n";
    return NULL;
    }

void Action::cfg_write( KConfigGroup& cfg_P ) const
    {
    cfg_P.writeEntry( "Type", "ERROR" ); // derived classes should call with their type
    }

// Action_list

Action_list::Action_list( KConfigGroup& cfg_P, Action_data* data_P )
    : Q3PtrList< Action >()
    {
    setAutoDelete( true );
    int cnt = cfg_P.readEntry( "ActionsCount", 0 );
    QString save_cfg_group = cfg_P.name();
    for( int i = 0;
         i < cnt;
         ++i )
        {
        KConfigGroup group( cfg_P.config(), save_cfg_group + QString::number( i ) );
        Action* action = Action::create_cfg_read( group, data_P );
        if( action )
            append( action );
        }
    }

void Action_list::cfg_write( KConfigGroup& cfg_P ) const
    {
    QString save_cfg_group = cfg_P.name();
    int i = 0;
    for( Iterator it( *this );
         it;
         ++it, ++i )
        {
        KConfigGroup group( cfg_P.config(), save_cfg_group + QString::number( i ) );
        it.current()->cfg_write( group );
        }
    cfg_P.writeEntry( "ActionsCount", i );
    }

// Command_url_action

Command_url_action::Command_url_action( KConfigGroup& cfg_P, Action_data* data_P )
    : Action( cfg_P, data_P )
    {
    _command_url = cfg_P.readEntry( "CommandURL" );
    }

void Command_url_action::cfg_write( KConfigGroup& cfg_P ) const
    {
    base::cfg_write( cfg_P );
    cfg_P.writeEntry( "CommandURL", command_url());
    cfg_P.writeEntry( "Type", "COMMAND_URL" ); // overwrites value set in base::cfg_write()
    }

void Command_url_action::execute()
    {
    if( command_url().isEmpty())
        return;
    KUriFilterData uri;
    QString cmd = command_url();
    static bool sm_ready = false;
    if( !sm_ready )
        {
        KWorkSpace::propagateSessionManager();
        sm_ready = true;
        }
//    int space_pos = command_url().find( ' ' );
//    if( command_url()[ 0 ] != '\'' && command_url()[ 0 ] != '"' && space_pos > -1
//        && command_url()[ space_pos - 1 ] != '\\' )
//        cmd = command_url().left( space_pos ); // get first 'word'
    uri.setData( cmd );
    KUriFilter::self()->filterUri( uri );
    if( uri.uri().isLocalFile() && !uri.uri().hasRef() )
        cmd = uri.uri().path();
    else
        cmd = uri.uri().url();
    switch( uri.uriType())
        {
        case KUriFilterData::LocalFile:
        case KUriFilterData::LocalDir:
        case KUriFilterData::NetProtocol:
        case KUriFilterData::Help:
            {
            ( void ) new KRun( uri.uri(),0L);
          break;
            }
        case KUriFilterData::Executable:
            {
            if (!KAuthorized::authorizeKAction("shell_access"))
		return;
            if( !uri.hasArgsAndOptions())
                {
                KService::Ptr service = KService::serviceByDesktopName( cmd );
                if( service )
                    {
                    KRun::run( *service, KUrl::List(), NULL );
                  break;
                    }
                }
            // fall though
            }
        case KUriFilterData::Shell:
            {
            if (!KAuthorized::authorizeKAction("shell_access"))
		return;
            if( !KRun::runCommand(
                cmd + ( uri.hasArgsAndOptions() ? uri.argsAndOptions() : "" ),
                cmd, uri.iconName(), NULL )) {
                // CHECKME ?
             }
          break;
            }
        default: // error
          return;
        }
    timeout.setSingleShot( true );
    timeout.start( 1000 ); // 1sec timeout

    }

const QString Command_url_action::description() const
    {
    return i18n( "Command/URL : " ) + command_url();
    }

Action* Command_url_action::copy( Action_data* data_P ) const
    {
    return new Command_url_action( data_P, command_url());
    }

// Menuentry_action

void Menuentry_action::cfg_write( KConfigGroup& cfg_P ) const
    {
    base::cfg_write( cfg_P );
    cfg_P.writeEntry( "Type", "MENUENTRY" ); // overwrites value set in base::cfg_write()
    }

KService::Ptr Menuentry_action::service() const
    {
    if (!_service)
    {
        const_cast<Menuentry_action *>(this)->_service = KService::serviceByStorageId(command_url());
    }
    return _service;
    }

void Menuentry_action::execute()
    {
    (void) service();
    if (!_service)
        return;
    KRun::run( *_service, KUrl::List(), 0 );
    timeout.setSingleShot( true );
    timeout.start( 1000 ); // 1sec timeout
    }

const QString Menuentry_action::description() const
    {
    (void) service();
    return i18n( "Menuentry : " ) + (_service ? _service->name() : QString());
    }

Action* Menuentry_action::copy( Action_data* data_P ) const
    {
    return new Menuentry_action( data_P, command_url());
    }

// Keyboard_input_action

Keyboard_input_action::Keyboard_input_action( KConfigGroup& cfg_P, Action_data* data_P )
    : Action( cfg_P, data_P )
    {
    _input = cfg_P.readEntry( "Input" );
    if( cfg_P.readEntry( "IsDestinationWindow" , false))
        {
        KConfigGroup windowGroup( cfg_P.config(), cfg_P.name() + "DestinationWindow" );
        _dest_window = new Windowdef_list( windowGroup );
        _active_window = false; // ignored with _dest_window set anyway
        }
    else
        {
        _dest_window = NULL;
        _active_window = cfg_P.readEntry( "ActiveWindow" , false);
        }
    }

Keyboard_input_action::~Keyboard_input_action()
    {
    delete _dest_window;
    }

void Keyboard_input_action::cfg_write( KConfigGroup& cfg_P ) const
    {
    base::cfg_write( cfg_P );
    cfg_P.writeEntry( "Type", "KEYBOARD_INPUT" ); // overwrites value set in base::cfg_write()
    cfg_P.writeEntry( "Input", input());
    if( dest_window() != NULL )
        {
        cfg_P.writeEntry( "IsDestinationWindow", true );
        KConfigGroup windowGroup( cfg_P.config(), cfg_P.name() + "DestinationWindow" );
        dest_window()->cfg_write( windowGroup );
        }
    else
        cfg_P.writeEntry( "IsDestinationWindow", false );
    cfg_P.writeEntry( "ActiveWindow", _active_window );
    }

void Keyboard_input_action::execute()
    {
    if( input().isEmpty())
        return;
    Window w = InputFocus;
    if( dest_window() != NULL )
        {
        w = windows_handler->find_window( dest_window());
        if( w == None )
            w = InputFocus;
        }
    else
        {
        if( !_active_window )
            w = windows_handler->action_window();
        if( w == None )
            w = InputFocus;
        }
    int last_index = -1, start = 0;
    while(( last_index = input().indexOf( ':', last_index + 1 )) != -1 ) // find next ';'
	{
        QString key = input().mid( start, last_index - start ).trimmed();
	keyboard_handler->send_macro_key( key, w );
	start = last_index + 1;
	}
    // and the last one
    QString key = input().mid( start, input().length()).trimmed();
    keyboard_handler->send_macro_key( key, w ); // the rest
    XFlush( QX11Info::display());
    }

const QString Keyboard_input_action::description() const
    {
    QString tmp = input();
    tmp.replace( '\n', ' ' );
    tmp.truncate( 30 );
    return i18n( "Keyboard input : " ) + tmp;
    }

Action* Keyboard_input_action::copy( Action_data* data_P ) const
    {
    return new Keyboard_input_action( data_P, input(),
        dest_window() ? dest_window()->copy() : NULL, _active_window );
    }

// Activate_window_action

Activate_window_action::Activate_window_action( KConfigGroup& cfg_P, Action_data* data_P )
    : Action( cfg_P, data_P )
    {
    QString save_cfg_group = cfg_P.name();
    KConfigGroup windowGroup( cfg_P.config(), save_cfg_group + "Window" );
    _window = new Windowdef_list( windowGroup );
    }

Activate_window_action::~Activate_window_action()
    {
    delete _window;
    }

void Activate_window_action::cfg_write( KConfigGroup& cfg_P ) const
    {
    base::cfg_write( cfg_P );
    cfg_P.writeEntry( "Type", "ACTIVATE_WINDOW" ); // overwrites value set in base::cfg_write()
    KConfigGroup windowGroup( cfg_P.config(), cfg_P.name() + "Window" );
    window()->cfg_write( windowGroup );
    }

void Activate_window_action::execute()
    {
    if( window()->match( windows_handler->active_window()))
        return; // is already active
    WId win_id = windows_handler->find_window( window());
    if( win_id != None )
        windows_handler->activate_window( win_id );
    }

const QString Activate_window_action::description() const
    {
    return i18n( "Activate window : " ) + window()->comment();
    }

Action* Activate_window_action::copy( Action_data* data_P ) const
    {
    return new Activate_window_action( data_P, window()->copy());
    }

} // namespace KHotKeys
