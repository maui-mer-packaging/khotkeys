/****************************************************************************

 KHotKeys

 Copyright (C) 1999-2001 Lubos Lunak <l.lunak@kde.org>

 Distributed under the terms of the GNU General Public License version 2.

****************************************************************************/

#define _TRIGGERS_TAB_CPP_

#include <config-khotkeys.h>

#include "triggers_tab.h"

#include <assert.h>
#include <QPushButton>
#include <QLineEdit>
#include <QMenu>
#include <QLayout>
#include <QLabel>
#include <Qt3Support/Q3Header>
#include <QVBoxLayout>

#include <kdebug.h>
#include <klocale.h>
#include <kapplication.h>
#include <kshortcut.h>
#include <kconfig.h>
#include <kkeysequencewidget.h>
#include <kvbox.h>

#include "kcmkhotkeys.h"
#include "windowdef_list_widget.h"
#include "window_trigger_widget.h"
#include "gesturerecordpage.h"
#include "voicerecordpage.h"

namespace KHotKeys
{

// Triggers_tab

Triggers_tab::Triggers_tab( QWidget* parent_P, const char* name_P )
    : Triggers_tab_ui( parent_P, name_P ), selected_item( NULL )
    {

    QMenu* popup = new QMenu; // CHECKME looks like setting parent doesn't work
    QAction *action = popup->addAction( i18n( "Shortcut Trigger..." ) );
    action->setData( TYPE_SHORTCUT_TRIGGER );

    action = popup->addAction( i18n( "Gesture Trigger..." ) );
    action->setData( TYPE_GESTURE_TRIGGER );

    action = popup->addAction( i18n( "Window Trigger..." ) );
    action->setData( TYPE_WINDOW_TRIGGER );

#ifdef HAVE_ARTS
    if( haveArts())
    {
        action = popup->addAction( i18n( "Voice Trigger..." ) );
        action->setData( TYPE_VOICE_TRIGGER );
    }
#endif


    connect( popup, SIGNAL( triggered( QAction* )), SLOT( new_selected( QAction* )));
    connect( triggers_listview, SIGNAL( doubleClicked ( Q3ListViewItem *, const QPoint &, int ) ),
             this, SLOT( modify_pressed() ) );

    new_button->setMenu( popup );
    copy_button->setEnabled( false );
    modify_button->setEnabled( false );
    delete_button->setEnabled( false );
    triggers_listview->header()->hide();
    triggers_listview->addColumn( "" );
    triggers_listview->setSorting( -1 );
    triggers_listview->setForceSelect( true );
    clear_data();
    // KHotKeys::Module::changed()
    connect( new_button, SIGNAL( clicked()),
        module, SLOT( changed()));
    connect( copy_button, SIGNAL( clicked()),
        module, SLOT( changed()));
    connect( modify_button, SIGNAL( clicked()),
        module, SLOT( changed()));
    connect( delete_button, SIGNAL( clicked()),
        module, SLOT( changed()));
    connect( comment_lineedit, SIGNAL( textChanged( const QString& )),
        module, SLOT( changed()));
    }

Triggers_tab::~Triggers_tab()
    {
    delete new_button->menu(); // CHECKME
    }

void Triggers_tab::clear_data()
    {
    comment_lineedit->clear();
    triggers_listview->clear();
    }

void Triggers_tab::set_data( const Trigger_list* data_P )
    {
    if( data_P == NULL )
        {
        clear_data();
        return;
        }
    comment_lineedit->setText( data_P->comment());
    Trigger_list_item* after = NULL;
    triggers_listview->clear();
    for( Trigger_list::Iterator it( *data_P );
         *it;
         ++it )
        after = create_listview_item( *it, triggers_listview, after, true );
    }

Trigger_list* Triggers_tab::get_data( Action_data* data_P ) const
    {
    Trigger_list* list = new Trigger_list( comment_lineedit->text());
    for( Q3ListViewItem* pos = triggers_listview->firstChild();
         pos != NULL;
         pos = pos->nextSibling())
        list->append( static_cast< Trigger_list_item* >( pos )->trigger()->copy( data_P ));
    return list;
    }

void Triggers_tab::new_selected( QAction *action )
    {
    Trigger_dialog* dlg = NULL;

    int type_P = action->data().toInt();
    switch( type_P )
        {
        case TYPE_SHORTCUT_TRIGGER: // Shortcut_trigger
            dlg = new Shortcut_trigger_dialog(
                new Shortcut_trigger( NULL, KShortcut())); // CHECKME NULL ?
          break;
        case TYPE_GESTURE_TRIGGER: // Gesture trigger
            dlg = new Gesture_trigger_dialog(
                new Gesture_trigger( NULL, QString() )); // CHECKME NULL ?
          break;
        case TYPE_WINDOW_TRIGGER: // Window trigger
            dlg = new Window_trigger_dialog( new Window_trigger( NULL, new Windowdef_list( "" ),
                0 )); // CHECKME NULL ?
          break;
        case TYPE_VOICE_TRIGGER: // Voice trigger
			dlg = new Voice_trigger_dialog( new Voice_trigger(NULL,QString(),VoiceSignature(),VoiceSignature())); // CHECKME NULL ?
			break;
        }
    if( dlg != NULL )
        {
        Trigger* trg = dlg->edit_trigger();
        if( trg != NULL )
            triggers_listview->setSelected( create_listview_item( trg, triggers_listview,
                selected_item, false ), true );
        delete dlg;
        }
    }

void Triggers_tab::copy_pressed()
    {
    triggers_listview->setSelected( create_listview_item( selected_item->trigger(),
        triggers_listview, selected_item, true ), true );
    }

void Triggers_tab::delete_pressed()
    {
    delete selected_item; // CHECKME snad vyvola signaly pro enable()
    }

void Triggers_tab::modify_pressed()
    {
    edit_listview_item( selected_item );
    }

void Triggers_tab::current_changed( Q3ListViewItem* item_P )
    {
//    if( item_P == selected_item )
//        return;
    selected_item = static_cast< Trigger_list_item* >( item_P );
//    triggers_listview->setSelected( item_P, true );
    copy_button->setEnabled( item_P != NULL );
    modify_button->setEnabled( item_P != NULL );
    delete_button->setEnabled( item_P != NULL );
    }

Trigger_list_item* Triggers_tab::create_listview_item( Trigger* trigger_P,
    Q3ListView* parent_P, Q3ListViewItem* after_P, bool copy_P )
    {
    Trigger* new_trg = copy_P ? trigger_P->copy( NULL ) : trigger_P; // CHECKME NULL ?
// CHECKME uz by nemelo byt treba    if( after_P == NULL )
//        return new Trigger_list_item( parent_P, new_trg );
//    else
        return new Trigger_list_item( parent_P, after_P, new_trg );
    }

void Triggers_tab::edit_listview_item( Trigger_list_item* item_P )
    {
    Trigger_dialog* dlg = NULL;
    if( Shortcut_trigger* trg = dynamic_cast< Shortcut_trigger* >( item_P->trigger()))
        dlg = new Shortcut_trigger_dialog( trg );
    else if( Gesture_trigger* trg = dynamic_cast< Gesture_trigger* >( item_P->trigger()))
        dlg = new Gesture_trigger_dialog( trg );
    else if( Window_trigger* trg = dynamic_cast< Window_trigger* >( item_P->trigger()))
        dlg = new Window_trigger_dialog( trg );
    else if( Voice_trigger* trg = dynamic_cast< Voice_trigger* >( item_P->trigger()))
        dlg = new Voice_trigger_dialog( trg );
// CHECKME TODO dalsi
    else
        assert( false );
    Trigger* new_trigger = dlg->edit_trigger();
    if( new_trigger != NULL )
        item_P->set_trigger( new_trigger );
    delete dlg;
    }

// Trigger_list_item

QString Trigger_list_item::text( int column_P ) const
    {
    return column_P == 0 ? trigger()->description() : QString();
    }

// Shortcut_trigger_widget

Shortcut_trigger_widget::Shortcut_trigger_widget( QWidget* parent_P, const char* )
    : QWidget( parent_P )
    {
    QVBoxLayout* lay = new QVBoxLayout( this );
    lay->setMargin( 11 );
    lay->setSpacing( 6 );
    QLabel* lbl = new QLabel( i18n( "Select keyboard shortcut:" ), this );
    lay->addWidget( lbl );
    lay->addSpacing( 10 );
    ksw = new KKeySequenceWidget( this );
    lay->addWidget( ksw, 0 , Qt::AlignHCenter );
    lay->addStretch();
    clear_data();
    connect( ksw, SIGNAL( capturedKeySequence( const QKeySequence& )),
        this, SLOT( capturedShortcut( const QKeySequence& )));
    }

void Shortcut_trigger_widget::clear_data()
    {
    //bt->setShortcut( KShortcut() );
    }

void Shortcut_trigger_widget::capturedShortcut( const QKeySequence& s_P )
    {
    /*if( KKeyChooser::checkGlobalShortcutsConflict( s_P, true, topLevelWidget())
        || KKeyChooser::checkStandardShortcutsConflict( s_P, true, topLevelWidget()))
        return;*/
    // KHotKeys::Module::changed()
    module->changed();
    //bt->setShortcut( s_P );
    }

void Shortcut_trigger_widget::set_data( const Shortcut_trigger* data_P )
    {
    if( data_P == NULL )
        {
        clear_data();
        return;
        }
    //bt->setShortcut( data_P->shortcut() );
    }

Shortcut_trigger* Shortcut_trigger_widget::get_data( Action_data* data_P ) const
    {
    /*return !bt->shortcut().isEmpty()
        ? new Shortcut_trigger( data_P, bt->shortcut()) : NULL;*/
    return 0;
    }

// Shortcut_trigger_dialog

Shortcut_trigger_dialog::Shortcut_trigger_dialog( Shortcut_trigger* trigger_P )
    : KDialog( 0 ), // CHECKME caption
        trigger( NULL )
    {
    setModal( true );
    setButtons( Ok | Cancel );
    widget = new Shortcut_trigger_widget( this );
    widget->set_data( trigger_P );
    setMainWidget( widget );
    }

Trigger* Shortcut_trigger_dialog::edit_trigger()
    {
    exec();
    return trigger;
    }

void Shortcut_trigger_dialog::accept()
    {
    KDialog::accept();
    trigger = widget->get_data( NULL ); // CHECKME NULL ?
    }

// Window_trigger_dialog

Window_trigger_dialog::Window_trigger_dialog( Window_trigger* trigger_P )
    : KDialog( 0 ), // CHECKME caption
        trigger( NULL )
    {
    setModal( true );
    setButtons( Ok | Cancel );
    widget = new Window_trigger_widget( this );
    widget->set_data( trigger_P );
    setMainWidget( widget );
    }

Trigger* Window_trigger_dialog::edit_trigger()
    {
    exec();
    return trigger;
    }

void Window_trigger_dialog::accept()
    {
    KDialog::accept();
    trigger = widget->get_data( NULL ); // CHECKME NULL ?
    }

// Gesture_trigger_dialog

Gesture_trigger_dialog::Gesture_trigger_dialog( Gesture_trigger* trigger_P )
    : KDialog( 0 ), // CHECKME caption
        _trigger( trigger_P ), _page( NULL )
    {
    setModal( true );
    setButtons( Ok | Cancel );
    _page = new GestureRecordPage( _trigger->gesturecode(),
                                  this, "GestureRecordPage");

    connect(_page, SIGNAL(gestureRecorded(bool)),
            this, SLOT(enableButtonOk(bool)));

    setMainWidget( _page );
    }

Trigger* Gesture_trigger_dialog::edit_trigger()
    {
    if( exec())
        return new Gesture_trigger( NULL, _page->getGesture()); // CHECKME NULL?
    else
        return NULL;
    }


// Voice_trigger_dialog

Voice_trigger_dialog::Voice_trigger_dialog( Voice_trigger* trigger_P )
: KDialog( ),
_trigger( trigger_P ), _page( NULL )
{
    setButtons( Ok | Cancel );
    // CHECKME caption
	_page = new VoiceRecordPage( _trigger ? _trigger->voicecode() : QString() ,  this);
        _page->setObjectName("VoiceRecordPage");

	connect(_page, SIGNAL(voiceRecorded(bool)), this, SLOT(enableButtonOK(bool)));

	setMainWidget( _page );
}

Trigger* Voice_trigger_dialog::edit_trigger()
{
	if( exec())
		return new Voice_trigger(NULL, _page->getVoiceId(),
								 (_page->isModifiedSignature(1) || !_trigger) ?  _page->getVoiceSignature(1) : _trigger->voicesignature(1) ,
								 (_page->isModifiedSignature(2) || !_trigger) ?  _page->getVoiceSignature(2) : _trigger->voicesignature(2) );
	else
		return NULL;
}




} // namespace KHotKeys

#include "triggers_tab.moc"
