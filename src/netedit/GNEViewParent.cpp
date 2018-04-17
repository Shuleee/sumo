/****************************************************************************/
// Eclipse SUMO, Simulation of Urban MObility; see https://eclipse.org/sumo
// Copyright (C) 2001-2018 German Aerospace Center (DLR) and others.
// This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v2.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v20.html
// SPDX-License-Identifier: EPL-2.0
/****************************************************************************/
/// @file    GNEViewParent.cpp
/// @author  Jakob Erdmann
/// @date    Feb 2011
/// @version $Id$
///
// A single child window which contains a view of the edited network (adapted
// from GUISUMOViewParent)
// While we don't actually need MDI for netedit it is easier to adapt existing
// structures than to write everything from scratch.
/****************************************************************************/


// ===========================================================================
// included modules
// ===========================================================================
#ifdef _MSC_VER
#include <windows_config.h>
#else
#include <config.h>
#endif

#include <string>
#include <vector>
#include <utils/foxtools/MFXUtils.h>
#include <utils/foxtools/MFXCheckableButton.h>
#include <utils/foxtools/MFXImageHelper.h>
#include <utils/common/UtilExceptions.h>
#include <utils/geom/Position.h>
#include <utils/geom/Boundary.h>
#include <utils/gui/globjects/GUIGlObjectTypes.h>
#include <utils/gui/globjects/GUIGlObjectStorage.h>
#include <utils/gui/images/GUIIcons.h>
#include <utils/gui/images/GUIIconSubSys.h>
#include <utils/gui/div/GUIDesigns.h>
#include <utils/gui/globjects/GUIGlObjectStorage.h>
#include <utils/gui/div/GUIIOGlobals.h>
#include <utils/gui/windows/GUIAppEnum.h>
#include <utils/gui/windows/GUIDialog_GLObjChooser.h>
#include <utils/common/MsgHandler.h>
#include <netedit/frames/GNETLSEditorFrame.h>
#include <netedit/frames/GNESelectorFrame.h>
#include <netedit/frames/GNEConnectorFrame.h>
#include <netedit/frames/GNETLSEditorFrame.h>
#include <netedit/frames/GNEAdditionalFrame.h>
#include <netedit/frames/GNECrossingFrame.h>
#include <netedit/frames/GNEDeleteFrame.h>
#include <netedit/frames/GNEPolygonFrame.h>
#include <netedit/frames/GNEInspectorFrame.h>
#include <netedit/netelements/GNEJunction.h>
#include <netedit/netelements/GNEEdge.h>
#include <netedit/netelements/GNELane.h>
#include <netedit/additionals/GNEAdditional.h>
#include <netedit/additionals/GNEPOI.h>
#include <netedit/additionals/GNEPoly.h>

#include "GNENet.h"
#include <netedit/netelements/GNEEdge.h>
#include "GNEViewNet.h"
#include "GNEViewParent.h"
#include "GNEUndoList.h"
#include "GNEApplicationWindow.h"


// ===========================================================================
// FOX callback mapping
// ===========================================================================
FXDEFMAP(GNEViewParent) GNEViewParentMap[] = {
    FXMAPFUNC(SEL_COMMAND,  MID_MAKESNAPSHOT,                       GNEViewParent::onCmdMakeSnapshot),
    //FXMAPFUNC(SEL_COMMAND,  MID_ALLOWROTATION,                    GNEViewParent::onCmdAllowRotation),
    FXMAPFUNC(SEL_COMMAND,  MID_LOCATEJUNCTION,                     GNEViewParent::onCmdLocate),
    FXMAPFUNC(SEL_COMMAND,  MID_LOCATEEDGE,                         GNEViewParent::onCmdLocate),
    FXMAPFUNC(SEL_COMMAND,  MID_LOCATETLS,                          GNEViewParent::onCmdLocate),
    FXMAPFUNC(SEL_COMMAND,  MID_LOCATEADD,                          GNEViewParent::onCmdLocate),
    FXMAPFUNC(SEL_COMMAND,  MID_LOCATEPOI,                          GNEViewParent::onCmdLocate),
    FXMAPFUNC(SEL_COMMAND,  MID_LOCATEPOLY,                         GNEViewParent::onCmdLocate),
    FXMAPFUNC(SEL_COMMAND,  FXMDIChild::ID_MDI_MENUCLOSE,           GNEViewParent::onCmdClose),
    FXMAPFUNC(SEL_CHANGED,  MID_GNE_VIEWPARENT_FRAMEAREAWIDTH,      GNEViewParent::onCmdUpdateFrameAreaWidth),
};

// Object implementation
FXIMPLEMENT(GNEViewParent, GUIGlChildWindow, GNEViewParentMap, ARRAYNUMBER(GNEViewParentMap))

// ===========================================================================
// member method definitions
// ===========================================================================
GNEViewParent::GNEViewParent(
    FXMDIClient* p, FXMDIMenu* mdimenu,
    const FXString& name,
    GNEApplicationWindow* parentWindow,
    FXGLCanvas* share, GNENet* net, GNEUndoList* undoList,
    FXIcon* ic, FXuint opts,
    FXint x, FXint y, FXint w, FXint h):
    GUIGlChildWindow(p, parentWindow, mdimenu, name, ic, opts, x, y, w, h),
    myGNEAppWindows(parentWindow) {
    // Add child to parent
    myParent->addChild(this, false);

    // disable coloring and screenshot
    //for (int i=5; i < myNavigationToolBar->numChildren(); i++) {
    //    myNavigationToolBar->childAtIndex(i)->hide();
    //}

    // add undo/redo buttons
    new FXButton(myNavigationToolBar, "\tUndo\tUndo the last Change.", GUIIconSubSys::getIcon(ICON_UNDO), parentWindow->getUndoList(), FXUndoList::ID_UNDO, GUIDesignButtonToolbar);
    new FXButton(myNavigationToolBar, "\tRedo\tRedo the last Change.", GUIIconSubSys::getIcon(ICON_REDO), parentWindow->getUndoList(), FXUndoList::ID_REDO, GUIDesignButtonToolbar);

    // Create Vertical separator
    new FXVerticalSeparator(myNavigationToolBar, GUIDesignVerticalSeparator);

    // Create Frame Splitter
    myFramesSplitter = new FXSplitter(myContentFrame, this, MID_GNE_VIEWPARENT_FRAMEAREAWIDTH, GUIDesignSplitter | SPLITTER_HORIZONTAL);

    // Create frames Area
    myFramesArea = new FXHorizontalFrame(myFramesSplitter, GUIDesignFrameArea);

    // Set default width of frames area
    myFramesArea->setWidth(220);

    // Create view area
    myViewArea = new FXHorizontalFrame(myFramesSplitter, GUIDesignViewnArea);

    // Add the view to a temporary parent so that we can add items to myViewArea in the desired order
    FXComposite* tmp = new FXComposite(this);

    // Create view net
    GNEViewNet* viewNet = new GNEViewNet(tmp, myViewArea, *myParent, this, net, undoList, myParent->getGLVisual(), share, myNavigationToolBar);

    // Set pointer myView with the created view net
    myView = viewNet;

    // Create frames
    myGNEFrames[MID_GNE_SETMODE_INSPECT] = new GNEInspectorFrame(myFramesArea, viewNet);
    myGNEFrames[MID_GNE_SETMODE_SELECT] = new GNESelectorFrame(myFramesArea, viewNet);
    myGNEFrames[MID_GNE_SETMODE_CONNECT] = new GNEConnectorFrame(myFramesArea, viewNet);
    myGNEFrames[MID_GNE_SETMODE_TLS] = new GNETLSEditorFrame(myFramesArea, viewNet);
    myGNEFrames[MID_GNE_SETMODE_ADDITIONAL] = new GNEAdditionalFrame(myFramesArea, viewNet);
    myGNEFrames[MID_GNE_SETMODE_CROSSING] = new GNECrossingFrame(myFramesArea, viewNet);
    myGNEFrames[MID_GNE_SETMODE_DELETE] = new GNEDeleteFrame(myFramesArea, viewNet);
    myGNEFrames[MID_GNE_SETMODE_POLYGON] = new GNEPolygonFrame(myFramesArea, viewNet);

    // Update frame areas after creation
    onCmdUpdateFrameAreaWidth(0, 0, 0);

    // Hidde all Frames Area
    hideFramesArea();

    //  Buld view toolBars
    myView->buildViewToolBars(*this);

    // create windows
    GUIGlChildWindow::create();
}


GNEViewParent::~GNEViewParent() {
    // Remove child before remove
    myParent->removeChild(this);
}


void
GNEViewParent::hideAllFrames() {
    for (auto i = myGNEFrames.begin(); i != myGNEFrames.end(); i++) {
        i->second->hide();
    }
}

GNEInspectorFrame*
GNEViewParent::getInspectorFrame() const {
    return dynamic_cast<GNEInspectorFrame*>(myGNEFrames.at(MID_GNE_SETMODE_INSPECT));
}


GNESelectorFrame*
GNEViewParent::getSelectorFrame() const {
    return dynamic_cast<GNESelectorFrame*>(myGNEFrames.at(MID_GNE_SETMODE_SELECT));
}


GNEConnectorFrame*
GNEViewParent::getConnectorFrame() const {
    return dynamic_cast<GNEConnectorFrame*>(myGNEFrames.at(MID_GNE_SETMODE_CONNECT));
}


GNETLSEditorFrame*
GNEViewParent::getTLSEditorFrame() const {
    return dynamic_cast<GNETLSEditorFrame*>(myGNEFrames.at(MID_GNE_SETMODE_TLS));
}


GNEAdditionalFrame*
GNEViewParent::getAdditionalFrame() const {
    return dynamic_cast<GNEAdditionalFrame*>(myGNEFrames.at(MID_GNE_SETMODE_ADDITIONAL));
}


GNECrossingFrame*
GNEViewParent::getCrossingFrame() const {
    return dynamic_cast<GNECrossingFrame*>(myGNEFrames.at(MID_GNE_SETMODE_CROSSING));
}


GNEDeleteFrame*
GNEViewParent::getDeleteFrame() const {
    return dynamic_cast<GNEDeleteFrame*>(myGNEFrames.at(MID_GNE_SETMODE_DELETE));
}


GNEPolygonFrame*
GNEViewParent::getPolygonFrame() const {
    return dynamic_cast<GNEPolygonFrame*>(myGNEFrames.at(MID_GNE_SETMODE_POLYGON));
}

void
GNEViewParent::showFramesArea() {
    bool showFlag = false;
    // Iterate over GNEFrames
    for (auto i = myGNEFrames.begin(); i != myGNEFrames.end(); i++) {
        // if at least one frame is shown, change showFlag
        if (i->second->shown()) {
            showFlag = true;
        }
    }
    // show and recalc framesArea if showFlag is enabled
    if (showFlag) {
        myFramesArea->recalc();
        myFramesArea->show();
    }
}


void
GNEViewParent::hideFramesArea() {
    bool hideFlag = true;
    // Iterate over frames
    for (auto i = myGNEFrames.begin(); i != myGNEFrames.end(); i++) {
        // if at least one frame is shown,  change hideflag
        if (i->second->shown()) {
            hideFlag = false;
        }
    }
    // hide and recalc frames Area if hideFlag is enabled
    if (hideFlag) {
        myFramesArea->hide();
        myFramesArea->recalc();
    }
}


GUIMainWindow*
GNEViewParent::getApp() const {
    return myParent;
}


GNEApplicationWindow*
GNEViewParent::getGNEAppWindows() const {
    return myGNEAppWindows;
}


long
GNEViewParent::onCmdMakeSnapshot(FXObject*, FXSelector, void*) {
    // get the new file name
    FXFileDialog opendialog(this, "Save Snapshot");
    opendialog.setIcon(GUIIconSubSys::getIcon(ICON_EMPTY));
    opendialog.setSelectMode(SELECTFILE_ANY);
    opendialog.setPatternList("All Image Files (*.gif, *.bmp, *.xpm, *.pcx, *.ico, *.rgb, *.xbm, *.tga, *.png, *.jpg, *.jpeg, *.tif, *.tiff, *.ps, *.eps, *.pdf, *.svg, *.tex, *.pgf)\n"
                              "GIF Image (*.gif)\nBMP Image (*.bmp)\nXPM Image (*.xpm)\nPCX Image (*.pcx)\nICO Image (*.ico)\n"
                              "RGB Image (*.rgb)\nXBM Image (*.xbm)\nTARGA Image (*.tga)\nPNG Image  (*.png)\n"
                              "JPEG Image (*.jpg, *.jpeg)\nTIFF Image (*.tif, *.tiff)\n"
                              "Postscript (*.ps)\nEncapsulated Postscript (*.eps)\nPortable Document Format (*.pdf)\n"
                              "Scalable Vector Graphics (*.svg)\nLATEX text strings (*.tex)\nPortable LaTeX Graphics (*.pgf)\n"
                              "All Files (*)");
    if (gCurrentFolder.length() != 0) {
        opendialog.setDirectory(gCurrentFolder);
    }
    if (!opendialog.execute() || !MFXUtils::userPermitsOverwritingWhenFileExists(this, opendialog.getFilename())) {
        return 1;
    }
    gCurrentFolder = opendialog.getDirectory();
    std::string file = opendialog.getFilename().text();
    std::string error = myView->makeSnapshot(file);
    if (error != "") {
        // write warning if netedit is running in testing mode
        if (OptionsCont::getOptions().getBool("gui-testing-debug")) {
            WRITE_WARNING("Opening FXMessageBox 'error saving snapshot'");
        }
        // open message box
        FXMessageBox::error(this, MBOX_OK, "Saving failed.", "%s", error.c_str());
        // write warning if netedit is running in testing mode
        if (OptionsCont::getOptions().getBool("gui-testing-debug")) {
            WRITE_WARNING("Closed FXMessageBox 'error saving snapshot' with 'OK'");
        }
    }
    return 1;
}


long
GNEViewParent::onCmdClose(FXObject*, FXSelector /* sel */, void*) {
    myParent->handle(this, FXSEL(SEL_COMMAND, MID_CLOSE), 0);
    return 1;
}


long
GNEViewParent::onCmdLocate(FXObject*, FXSelector sel, void*) {
    GNEViewNet* view = dynamic_cast<GNEViewNet*>(myView);
    assert(view);
    GUIIcon icon;
    std::string dialogtitle;
    std::vector<GUIGlID> ids;
    switch (FXSELID(sel)) {
        case MID_LOCATEJUNCTION: {
            // fill ids with junctions
            std::vector<GNEJunction*> junctions = view->getNet()->retrieveJunctions();
            for(auto i : junctions) {
                ids.push_back(i->getGlID());
            }
            icon = ICON_LOCATEJUNCTION;
            dialogtitle = "Junction Chooser";
            break;
        }
        case MID_LOCATEEDGE: {
            // fill ids with edges
            std::vector<GNEEdge*> edges = view->getNet()->retrieveEdges();
            for(auto i : edges) {
                ids.push_back(i->getGlID());
            }
            icon = ICON_LOCATEEDGE;
            dialogtitle = "Edge Chooser";
            break;
        }
        case MID_LOCATETLS: {
            // fill ids with junctions that haven TLS
            std::vector<GNEJunction*> junctions = view->getNet()->retrieveJunctions();
            for(auto i : junctions) {
                if(i->getNBNode()->getControllingTLS().size() > 0) {
                    ids.push_back(i->getGlID());
                }
            }
            icon = ICON_LOCATETLS;
            dialogtitle = "TLS Chooser";
            break;
        }
        case MID_LOCATEADD: {
            // fill ids with additionals
            std::vector<GNEAdditional*> additionals = view->getNet()->retrieveAdditionals();
            for(auto i : additionals) {
                ids.push_back(i->getGlID());
            }
            icon = ICON_LOCATEADD;
            dialogtitle = "Additional Chooser";
            break;
        }
        case MID_LOCATEPOI: {
            // fill ids with POIs
            for(auto i : view->getNet()->getPOIs()) {
                ids.push_back(dynamic_cast<GNEPOI*>(i.second)->getGlID());
            }
            icon = ICON_LOCATEPOI;
            dialogtitle = "Junction Chooser";
            break;
        }
        case MID_LOCATEPOLY: {
            // fill ids with polys
            for(auto i : view->getNet()->getPolygons()) {
                ids.push_back(dynamic_cast<GNEPoly*>(i.second)->getGlID());
            }
            icon = ICON_LOCATEPOLY;
            dialogtitle = "Poly Chooser";
            break;
        }
        default:
            throw ProcessError("Unknown Message ID in onCmdLocate");
    }
    myLocatorPopup->popdown();
    myLocatorButton->killFocus();
    myLocatorPopup->update();
    GUIDialog_GLObjChooser* chooser = new GUIDialog_GLObjChooser(
        this, GUIIconSubSys::getIcon(icon), dialogtitle.c_str(), ids, GUIGlObjectStorage::gIDStorage);
    chooser->create();
    chooser->show();
    return 1;
}


long
GNEViewParent::onKeyPress(FXObject* o, FXSelector sel, void* eventData) {
    myView->onKeyPress(o, sel, eventData);
    return 0;
}


long
GNEViewParent::onKeyRelease(FXObject* o, FXSelector sel, void* eventData) {
    myView->onKeyRelease(o, sel, eventData);
    return 0;
}


long
GNEViewParent::onCmdUpdateFrameAreaWidth(FXObject*, FXSelector, void*) {
    for (auto i = myGNEFrames.begin(); i != myGNEFrames.end(); i++) {
        // update size of all GNEFrame
        i->second->setFrameWidth(myFramesArea->getWidth());
    }
    return 0;
}

/****************************************************************************/

