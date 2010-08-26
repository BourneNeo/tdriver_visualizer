/***************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (testabilitydriver@nokia.com)
**
** This file is part of Testability Driver.
**
** If you have questions regarding the use of this file, please contact
** Nokia at testabilitydriver@nokia.com .
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation
** and appearing in the file LICENSE.LGPL included in the packaging
** of this file.
**
****************************************************************************/


#include "tdriver_main_window.h"
#include "tdriver_recorder.h"

#include <tdriver_tabbededitor.h>
#include <tdriver_rubyinterface.h>
#include "../common/version.h"

#include <QApplication>
#include <QCloseEvent>

#include <tdriver_debug_macros.h>


// Performs post-initialization checking.
void MainWindow::checkInit()
{
    // check if RUBYOPT has been set
    QString current( getenv( tr( "RUBYOPT" ).toLatin1().data() ) );
    if ( current != "rubygems" ) { putenv( tr( "RUBYOPT=rubygems" ).toLatin1().data() ); }
}


bool MainWindow::checkVersion( QString currentVersion, QString requiredVersion )
{
    // convert required version to array
    QStringList tmpRequiredVersionArray = requiredVersion.split( QRegExp("[.-]") );

    // convert installed driver version to array
    QStringList tmpDriverVersionArray = currentVersion.split( QRegExp("[.-]") );

    // make version arrays same length
    while ( tmpDriverVersionArray.count() < tmpRequiredVersionArray.count() ) {
        tmpDriverVersionArray.append("0");
    }

    bool versionOk = true;

    for ( int index = 0; index < tmpRequiredVersionArray.count();  index++ ) {
        int current_version = tmpDriverVersionArray.at( index ).toInt();
        int required_version = tmpRequiredVersionArray.at( index ).toInt();

        // check if installed version is new enough
        if ( current_version > required_version ) {
            break;
        }
        else {
            if ( current_version < required_version ){
                versionOk = false;
                break;
            }
        }
    }

    return versionOk;
}


// Function entered when creating Visulizer. Sets default and calls helper functions
// to setup UI of Visualizer
bool MainWindow::setup()
{
    setObjectName("main");
    QTime t;  // for performance debugging, can be removed

    // read visualizer settings from visualizer.ini file
    //applicationSettings = new QSettings( QApplication::applicationDirPath() + "/visualizer.ini", QSettings::IniFormat );
    applicationSettings = new QSettings(QSettings::IniFormat, QSettings::UserScope, "Nokia", "TDriver_Visualizer");

    TDriverRubyInterface::startGlobalInstance();

    // determine if connection to TDriver established -- if not, allow user to run TDriver Visualizer in viewer/offline mode
    offlineMode = true;
    t.start();
    if ( TDriverRubyInterface::globalInstance()->goOnline()) {
        QString installedDriverVersion = getDriverVersionNumber();

        if ( !checkVersion( installedDriverVersion, REQUIRED_DRIVER_VERSION ) ) {

            QMessageBox::critical(
                    0,
                    tr("TDriver Visualizer v") + VISUALIZER_VERSION,
                    tr("TDriver Visualizer is not compatible with this version of TDriver. Please update your TDriver environment.\n\n") +
                    tr("Installed version: ") + installedDriverVersion +
                    tr("\nRequired version: ") + REQUIRED_DRIVER_VERSION + tr(" or later")+
                    tr("\n\nLaunching in offline mode.")
                    );
        }
        else {
            offlineMode = false; // TDriver successfully initialized!
        }
    }
    qDebug() << FCFL << "RBI goOnline  result" << !offlineMode << "secs" << float(t.elapsed())/1000.0;


    if (offlineMode) {
        qWarning("Failed to initialize TDriver, closing Ruby process");
        TDriverRubyInterface::globalInstance()->requestClose();
    }

    tdriverPath = applicationSettings->value( "files/location", "" ).toString();

    // default font for QTableWidgetItems and QTreeWidgetItems
    defaultFont = new QFont;
    defaultFont->fromString(  applicationSettings->value( "font/settings", QString("Sans Serif,8,-1,5,50,0,0,0,0,0") ).toString() );
    emit defaultFontSet(*defaultFont);

    // default window size & position
    QPoint windowPosition = applicationSettings->value( "window/pos", QPoint( 200, 200 ) ).toPoint();
    QSize windowSize = applicationSettings->value( "window/size", QSize( 950, 600 ) ).toSize();

    // image resize
    bool resizeImage = applicationSettings->value( "image/resize", true ).toBool();
    // read dock visibility settings
    bool imageVisible = applicationSettings->value( "view/image", true ).toBool();
    bool clipboardVisible = applicationSettings->value( "view/clipboard", true ).toBool();
    bool propertiesVisible = true; //applicationSettings->value( "view/properties", true ).toBool();
    bool buttonVisible = applicationSettings->value( "view/buttons", false ).toBool();
    bool shortcutsVisible = applicationSettings->value( "view/shortcuts", false ).toBool();
    bool editorVisible = applicationSettings->value( "view/editor", false ).toBool();

    // default sut
    QString defaultDevice = applicationSettings->value( "sut/activesut", QString( "sut_qt" ) ).toString();

    // if default sut is empty (in ini file), set it as 'sut_qt'
    if ( defaultDevice.isEmpty() ) { defaultDevice = "sut_qt"; }

    // initialize & default settings

    if ( tdriverPath.isEmpty() ) {
        // construct QString tdriverPath depending on OS
#if (defined(Q_OS_WIN32))
        tdriverPath = "c:/tdriver/";
#else
        tdriverPath = "/etc/tdriver/";
#endif
    }

    // set current parameters xml file to be used
    while ( !QFile((parametersFile = tdriverPath + "/tdriver_parameters.xml")).exists() ) {

        QMessageBox::StandardButton result = QMessageBox::critical(
                0,
                tr("Missing file"),
                tr("Could not locate TDriver parameters file:\n\n  %1\n\n").arg(parametersFile) +
                tr("Please click Ok to select correct folder, or Cancel to quit.\n\nNote: Location will be saved to Visualizer configuration."),
                QMessageBox::Ok | QMessageBox::Cancel);

        if (result & QMessageBox::Cancel) {
            return false; // exit
        }

        tdriverPath = selectFolder( "Select TDriver configuration file folder", "Folder", QFileDialog::AcceptOpen ) + "/";
    }
    applicationSettings->setValue( "files/location", tdriverPath );

    // object tree
    collapsedObjectTreeItemPtr = 0;
    expandedObjectTreeItemPtr = 0;
    lastHighlightedObjectPtr = -1;

    // clear application process id list, used when selecting application from menu
    applicationsProcessIdMap.clear();

    // clear object attributes map
    attributesMap.clear();

    // clear current application map
    currentApplication.clear();

    // clear behaviours cache
    behavioursMap.clear();

    /* initialize help, context sensitivity needs watch for events */
    installEventFilter( this );
    //tdriverAssistant = new Assistant;

    // create tdriver recorder
    mRecorder = new TDriverRecorder( this );

    // create show xml dialog
    createXMLFileDataWindow();

    // create user interface
    createUi();

    // create find dialog
    createFindDialog();

    // parse parameters xml to retrieve all devices
    if ( !offlineMode ){
        offlineMode = getXmlParameters( parametersFile );

        if ( !offlineMode && deviceList.count() > 0 ) {
            deviceMenu->setEnabled( true );
            disconnectCurrentSUT->setEnabled( true );
            refreshAction->setEnabled( true );
            parseSUT->setEnabled( true );
        }
        tabEditor->setParamMap(tdriverXmlParameters);
    }

    // parse behaviours xml
    //parseBehavioursXml( tdriverPath + "/behaviours.xml" );

    // default sut
    setActiveDevice( defaultDevice );

    // update main window title - current sut will be shown
    updateWindowTitle();

    // set image view resize checkbox setting
    checkBoxResize->setCheckState( resizeImage ? Qt::Checked : Qt::Unchecked  );
    changeImageResize();

    connect( clipboardDock, SIGNAL( visibilityChanged( bool ) ), this, SLOT( visiblityChangedClipboard( bool ) ) );
    connect( imageViewDock, SIGNAL( visibilityChanged( bool ) ), this, SLOT( visiblityChangedImage( bool ) ) );
    connect( propertiesDock, SIGNAL( visibilityChanged( bool ) ), this, SLOT( visiblityChangedProperties( bool ) ) );
    connect( shortcutsDock, SIGNAL( visibilityChanged( bool ) ), this, SLOT( visiblityChangedShortcuts( bool ) ) );
    connect( editorDock, SIGNAL( visibilityChanged( bool ) ), this, SLOT( visiblityChangedEditor( bool ) ) );
    connect( keyboardCommandsDock, SIGNAL( visibilityChanged( bool ) ), this, SLOT( visiblityChangedButtons( bool ) ) );

    // set visibilities
    clipboardDock->setVisible( clipboardVisible );
    imageViewDock->setVisible( imageVisible );
    propertiesDock->setVisible( propertiesVisible );
    keyboardCommandsDock->setVisible( buttonVisible );
    shortcutsDock->setVisible( shortcutsVisible );
    editorDock->setVisible( editorVisible );

    // resize window
    resize( windowSize );
    move( windowPosition );

    connectSignals();

    // xml/screen capture output path depending on OS
#if (defined(Q_OS_WIN32))
    outputPath = QString( getenv( "TEMP" ) ) + "/";
#else
    outputPath = "/tmp/";
#endif

    if ( !offlineMode &&
         !execute_command( commandSetOutputPath, "listener set_output_path " + outputPath) ) {
        outputPath = QApplication::applicationDirPath();
    }

    deviceSelected();
    return true;

} // setup


void MainWindow::setActiveDevice( QString deviceName )
{
    activeDevice.clear();
    QHash<QString, QString> sut;

    if ( deviceList.contains( deviceName ) ) {
        sut = deviceList.value( deviceName );

        activeDevice["name"] = sut.value( "name" );
        activeDevice["type"] = sut.value( "type" );

        // Eisable record menu if active device type is kind of QT
        if ( sut.value( "type" ).contains( "qt", Qt::CaseInsensitive ) ) {
            recordMenu->setEnabled( !applicationsHash.empty() );
        }
    }
}

QString MainWindow::getDriverVersionNumber()
{
#if 1
    QString ver(TDriverRubyInterface::globalInstance()->getTDriverVersion());
    qDebug() << FCFL << "got version" << ver;
    return  (ver.isEmpty()) ? "Unknown" : ver;
#else
    QByteArray result = "Unknown";
    BAListMap reply;

    if ( execute_command( commandGetVersionNumber, "listener check_version", "", &reply ) ) {
        result = cleanDoneResult(reply["OUTPUT"].first());
    }

    qDebug() << FCFL << "GOT VERSION" << result;
    return QString(result);
#endif
}

QByteArray MainWindow::cleanDoneResult(QByteArray output)
{
    int doneIndex = output.indexOf("done");
    if ( doneIndex > -1 ) {
        // remove done from the end of string
        output.truncate(doneIndex);
    }
    // remove linefeed characters
    return output.trimmed();
}


QString MainWindow::getDeviceType( QString deviceName )
{
    QByteArray result = "Unknown";
    BAListMap reply;
    QString command = deviceName + " get_parameter type";

    if ( execute_command( commandGetDeviceType, command, deviceName, &reply ) ) {
        result = cleanDoneResult(reply["OUTPUT"].first());
    }

    return QString(result);
}


void MainWindow::connectSignals()
{
    connectObjectTreeSignals();
    connectTabWidgetSignals();
    connectImageWidgetSignals();

    QMetaObject::connectSlotsByName( this );
}

// This is called when closing Visualizer. Call threads close method, which
// shuts down thread (closes and then kills it)
void MainWindow::closeEvent( QCloseEvent *event )
{
    if (tabEditor && !tabEditor->mainCloseEvent(event)) {
        qDebug() << "closeEvent rejected by code editor";
        event->ignore();
        return;
    }

    // close show xml window if still visible
    //if ( xmlView->isVisible() ) { xmlView->close();    }

    // save tdriver path
    applicationSettings->setValue( "files/location", tdriverPath );

    // default window size & position
    applicationSettings->setValue("window/pos", pos());
    applicationSettings->setValue("window/size", size());

    // default sut
    applicationSettings->setValue("sut/activesut", activeDevice.value("name") );
    applicationSettings->setValue("sut/activesuttype", activeDevice.value("type") );

    // object tree settings
    //for ( int i = 0; i < 3 ; i++ ) { applicationSettings->setValue( QString("objecttree/column" + QString::number( i ) ), objectTree->columnWidth( i ) ); }

    // image settings
    applicationSettings->setValue("image/resize", checkBoxResize->isChecked());

    // clipboard contents
    applicationSettings->setValue( "view/clipboard", viewClipboard->isChecked() );
    applicationSettings->setValue( "view/image", viewImage->isChecked() );
    applicationSettings->setValue( "view/properties", viewProperties->isChecked() );
    applicationSettings->setValue( "view/buttons", viewButtons->isChecked() );
    applicationSettings->setValue( "view/shortcuts", viewShortcuts->isChecked() );
    applicationSettings->setValue( "view/editor", viewEditor->isChecked() );

    applicationSettings->setValue( "font/settings", defaultFont->toString() );

    TDriverRubyInterface::globalInstance()->requestClose();
}

// Event filter, catches F1/HELP key events and processes them, calling Assistant to display the corresponding help page.
bool MainWindow::eventFilter(QObject * object, QEvent *event) {

    Q_UNUSED( object );

    if (event->type() == QEvent::KeyPress) {

        QKeyEvent *ke = static_cast<QKeyEvent *>(event);

        //qDebug() << "modifiers(): " << ke->modifiers() << ", key(): " << ke->key();

        if (ke->key() == Qt::Key_F1 || ke->key() == Qt::Key_Help) {

            //QWidget *widget = 0;
            QString page = "qdoc-temp/index.html";
            /* Context sensitivity disabled for now
                You need to also remove the line "visualizerAssistant->setShortcut(tr("F1"));" from the createTopMenuBar method to enable processing of F1 ket events in this event handler
            if (object->isWidgetType()) {

                widget = static_cast<QWidget *>(object)-> focusWidget();
                if (widget == objectTree)
                    page = "tree.html";
                }else {
                    page = "devices.html";
                }
            */
            showContextVisualizerAssistant(page);

            return true;
        }
    }
    return QMainWindow::eventFilter(object, event);
}


// MainWindow listener for keypresses
void MainWindow::keyPressEvent ( QKeyEvent * event )
{
    // qDebug() << "MainWindow::keyPressEvent: " << event->key();
    if ( QApplication::focusWidget() == objectTree && objectTree->currentItem() != NULL )
        objectTreeKeyPressEvent( event );
    else
        event->ignore();

    collapsedObjectTreeItemPtr = 0;
    expandedObjectTreeItemPtr = 0;
}


bool MainWindow::isDeviceSelected()
{
    return !activeDevice.isEmpty();
}


void MainWindow::noDeviceSelectedPopup()
{
    if ( !offlineMode ){
        QMessageBox::critical(0, tr( "Error" ), "Unable to refresh due to no device selected.\n\nPlease select one from devices menu." );
    }
}


QString MainWindow::selectFolder( QString title, QString filter, QFileDialog::AcceptMode mode ) {

    QFileDialog dialog( this );

    dialog.setAcceptMode( mode );
    dialog.setFileMode( QFileDialog::Directory );
    dialog.setNameFilter( filter );
    dialog.setWindowTitle( title );
    dialog.setViewMode( QFileDialog::List );

    return ( dialog.exec() ? dialog.selectedFiles().at( 0 ) : "" );

}

void MainWindow::statusbar( QString text, int timeout ) {

    statusBar()->showMessage( text, timeout );

    statusBar()->update();
    statusBar()->repaint();

}

void MainWindow::statusbar( QString text, int currentProgressValue, int maxProgressValue, int timeout ) {

    int progress = int( (  float( currentProgressValue ) / float( maxProgressValue ) ) * float( 100 ) );

    statusBar()->showMessage( text + " " + QString::number( progress ) + "%", timeout );

    statusBar()->update();
    statusBar()->repaint();

}

bool MainWindow::processErrorMessage( QString & resultMessage, ExecuteCommandType commandType, int & resultEnum ) {

    QString errorMessage = resultMessage;

    resultEnum = FAIL;

    bool result = false;

    switch ( commandType ) {

    case commandCheckApiFixture:

        resultEnum = SILENT;
        break;

    default:

        if ( errorMessage.contains( "No plugins and no ui for server" ) ) {

            resultMessage = tr( "Failed to refresh application screen capture.\n\nLaunch some application with UI and try again." );
            resultEnum = WARNING;

        } else if( errorMessage.contains( "No connection could be made because the target machine actively refused it." ) ) {

            resultMessage = tr( "Please start/restart QTTAS server." );
            resultEnum = DISCONNECT + RETRY;

        } else if( errorMessage.contains( "An existing connection was forcibly closed by the remote host." ) ) {

            resultMessage = tr( "Please disconnect the SUT from file menu and try again.\n\nIf the problem persists, restart QTTAS server/device or contact support." );
            resultEnum = DISCONNECT + RETRY;

        } else if( errorMessage.contains( "Connection refused" ) ) {

            resultMessage = tr( "Unable to connect to target. Please verify that required servers are running and target is connected properly.\n\nIf the problem persists, contact support." );
            resultEnum = DISCONNECT + RETRY;

        } else if( errorMessage.contains( "No data retrieved (IOError)" ) ) {

            resultMessage = tr( "Unable to read data from target. Please verify that required servers are running and target is connected properly.\n\nIf the problem persists, contact support." );
            resultEnum = DISCONNECT + RETRY;

        } else if( errorMessage.contains( "Broken pipe (Errno::EPIPE)" ) ) {

            resultMessage = tr( "Unable to connect to target due to broken pipe.\n\nPlease disconnect SUT, verify that required servers are running/target is connected properly and try again.\n\nIf the problem persists, contact support." );

            resultEnum = DISCONNECT + RETRY;

        } else {
            // unknown error
            resultMessage = tr( "Unexpected error, please see details." ); // tr( QString( errorMessage ).toLatin1() );

        }

        break;

    }

    return result;

}


bool MainWindow::execute_command( ExecuteCommandType commandType, QString commandString, QString additionalInformation, BAListMap *reply )
{
    QString errorPrefix;
    QString errorMessage;
    bool exit = false;
    bool result = true;
    int iteration = 0;
    int resultEnum;
    QString originalErrorMessage;

    do {
        BAListMap msg;
        msg["input"] = commandString.toAscii().split(' ');

        qDebug() << FCFL << "going to execute" << msg;
        QTime t;
        t.start();
        if ( !TDriverRubyInterface::globalInstance()->executeCmd("listener.rb emulation", msg, 30000 )) {
            qDebug() << FCFL << "failure time" << float(t.elapsed())/1000.0 << "reply" << msg;
            if (msg["OUTPUT"].isEmpty()) msg["OUTPUT"].append("");
            errorMessage = msg.value("OUTPUT").first();

            // store original error message for details box
            originalErrorMessage = errorMessage;

            result = false;
            exit = true;

            switch ( commandType ) {
            case commandListApps: errorPrefix = "Error retrieving applications list:\n\n"; break;
            case commandClassMethods: errorPrefix = "Error retrieving methods list for " + additionalInformation + ".\n\n"; break;
            case commandSignalList: errorPrefix = "Error retrieving signal list for " + additionalInformation + ".\n\n"; break;
            case commandDisconnectSUT: errorPrefix = "Error disconnecting SUT '" + additionalInformation + "'\n\n"; break;
            case commandTapScreen: errorPrefix = "Error performing tap to screen\n\n"; break;
            case commandRefreshUI: errorPrefix = "Failed to refresh application\n\n"; break;
            case commandKeyPress: errorPrefix = "Failed to press key '" + additionalInformation + "'.\n\n"; break;
            case commandSetAttribute: errorPrefix = "Failed to set attribute '" + additionalInformation + "'.\n\n"; break;
            case commandGetDeviceType: errorPrefix = "Failed to get device type for " + additionalInformation + ".\n\n"; break;
            case commandGetVersionNumber: errorPrefix = "Failed to retrieve TDriver version number.\n\n"; break;
            default: errorPrefix = "Error: Failed to execute command \""+ commandString + "\".\n\n";
            }

            result = processErrorMessage( errorMessage, commandType, resultEnum );
            errorMessage = errorPrefix + errorMessage;
            qCritical( "MainWindow::execute_command failed. Error: %s", qPrintable(errorMessage) );

            if ( resultEnum & FAIL || iteration > 0 ) {
                // exit if failed again or no retries allowed..
                exit = true;
            }

            else {
                if ( resultEnum & DISCONNECT ) {
                    // disconnect
                    msg.clear();
                    msg["input"] << activeDevice.value( "name" ).toAscii() << "disconnect";
                    if ( !TDriverRubyInterface::globalInstance()->executeCmd("listener.rb emulation", msg, 5000 ) ) {
                        if (msg["OUTPUT"].isEmpty()) msg["OUTPUT"].append("");
                        errorMessage = msg.value("OUTPUT").first();
                        // disconnect failed -- exit
                        result = processErrorMessage( errorMessage, commandType, resultEnum );
                        errorMessage = errorPrefix + errorMessage;
                        result = false;
                        exit = true;
                    }
                    else {
                        // disconnect passed -- retry
                        qFatal("reconnect after disconnect in %s broken", __FUNCTION__);
                        currentApplication.clear();
                        result = false;
                        exit = false;
                    }
                }
            }
        }
        else {
            qDebug() << FCFL << "success time" << float(t.elapsed())/1000.0 << "reply" << msg;
            if (reply) {
                *reply = msg;
            }
            result = true;
            exit = true;
        }

        iteration++;
    } while (!exit);

    if ( !result && !(resultEnum & SILENT) ) {
        QMessageBox msgBox( this );
        msgBox.setIcon( QMessageBox::Critical );
        msgBox.setWindowTitle( "Error" );
        msgBox.setText( errorMessage );

        if( originalErrorMessage.length() > 0 ){
            msgBox.setDetailedText( originalErrorMessage );
        }

        msgBox.setStandardButtons( QMessageBox::Ok );
        msgBox.exec();
    }


    return result;
}

