/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2019-2023 CERN
 * Copyright (C) 2019-2024 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "sch_sheet_path.h"
#include <memory>

#include <kiplatform/ui.h>
#include <optional>
#include <project_sch.h>
#include <tools/sch_drawing_tools.h>
#include <tools/sch_line_wire_bus_tool.h>
#include <tools/ee_selection_tool.h>
#include <tools/ee_grid_helper.h>
#include <tools/rule_area_create_helper.h>
#include <gal/graphics_abstraction_layer.h>
#include <ee_actions.h>
#include <sch_edit_frame.h>
#include <pgm_base.h>
#include <eeschema_id.h>
#include <confirm.h>
#include <view/view_controls.h>
#include <view/view.h>
#include <sch_symbol.h>
#include <sch_no_connect.h>
#include <sch_line.h>
#include <sch_junction.h>
#include <sch_bus_entry.h>
#include <sch_rule_area.h>
#include <sch_text.h>
#include <sch_textbox.h>
#include <sch_table.h>
#include <sch_tablecell.h>
#include <sch_sheet.h>
#include <sch_sheet_pin.h>
#include <sch_label.h>
#include <sch_bitmap.h>
#include <schematic.h>
#include <sch_commit.h>
#include <scoped_set_reset.h>
#include <symbol_library.h>
#include <eeschema_settings.h>
#include <dialogs/dialog_label_properties.h>
#include <dialogs/dialog_text_properties.h>
#include <dialogs/dialog_wire_bus_properties.h>
#include <dialogs/dialog_junction_props.h>
#include <dialogs/dialog_table_properties.h>
#include <import_gfx/dialog_import_gfx_sch.h>
#include <sync_sheet_pin/sheet_synchronization_agent.h>
#include <string_utils.h>
#include <wildcards_and_files_ext.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>

SCH_DRAWING_TOOLS::SCH_DRAWING_TOOLS() :
        EE_TOOL_BASE<SCH_EDIT_FRAME>( "eeschema.InteractiveDrawing" ),
        m_lastSheetPinType( LABEL_FLAG_SHAPE::L_INPUT ),
        m_lastGlobalLabelShape( LABEL_FLAG_SHAPE::L_INPUT ),
        m_lastNetClassFlagShape( LABEL_FLAG_SHAPE::F_ROUND ),
        m_lastTextOrientation( SPIN_STYLE::RIGHT ),
        m_lastTextBold( false ),
        m_lastTextItalic( false ),
        m_lastTextAngle( ANGLE_0 ),
        m_lastTextboxAngle( ANGLE_0 ),
        m_lastTextHJustify( GR_TEXT_H_ALIGN_CENTER ),
        m_lastTextVJustify( GR_TEXT_V_ALIGN_CENTER ),
        m_lastTextboxHJustify( GR_TEXT_H_ALIGN_LEFT ),
        m_lastTextboxVJustify( GR_TEXT_V_ALIGN_TOP ),
        m_lastFillStyle( FILL_T::NO_FILL ),
        m_lastTextboxFillStyle( FILL_T::NO_FILL ),
        m_lastFillColor( COLOR4D::UNSPECIFIED ),
        m_lastTextboxFillColor( COLOR4D::UNSPECIFIED ),
        m_lastStroke( 0, LINE_STYLE::DEFAULT, COLOR4D::UNSPECIFIED ),
        m_lastTextboxStroke( 0, LINE_STYLE::DEFAULT, COLOR4D::UNSPECIFIED ),
        m_mruPath( wxEmptyString ),
        m_lastAutoLabelRotateOnPlacement( false ),
        m_drawingRuleArea( false ),
        m_inDrawingTool( false )
{
}


bool SCH_DRAWING_TOOLS::Init()
{
    EE_TOOL_BASE::Init();

    auto belowRootSheetCondition =
            [&]( const SELECTION& aSel )
            {
                return m_frame->GetCurrentSheet().Last() != &m_frame->Schematic().Root();
            };

    auto inDrawingRuleArea =
            [&]( const SELECTION& aSel )
            {
                return m_drawingRuleArea;
            };

    CONDITIONAL_MENU& ctxMenu = m_menu.GetMenu();
    ctxMenu.AddItem( EE_ACTIONS::leaveSheet, belowRootSheetCondition, 150 );
    ctxMenu.AddItem( EE_ACTIONS::closeOutline, inDrawingRuleArea, 200 );
    ctxMenu.AddItem( EE_ACTIONS::deleteLastPoint, inDrawingRuleArea, 200 );

    return true;
}


int SCH_DRAWING_TOOLS::PlaceSymbol( const TOOL_EVENT& aEvent )
{
    SCH_SYMBOL*                 symbol = aEvent.Parameter<SCH_SYMBOL*>();
    SYMBOL_LIBRARY_FILTER       filter;
    std::vector<PICKED_SYMBOL>* historyList = nullptr;
    bool                        ignorePrimePosition = false;
    COMMON_SETTINGS*            common_settings = Pgm().GetCommonSettings();
    SCH_SCREEN*                 screen = m_frame->GetScreen();

    if( m_inDrawingTool )
        return 0;

    REENTRANCY_GUARD guard( &m_inDrawingTool );

    KIGFX::VIEW_CONTROLS* controls = getViewControls();
    EE_GRID_HELPER        grid( m_toolMgr );
    VECTOR2I              cursorPos;

    // First we need to get all instances of this sheet so we can annotate
    // whatever symbols we place on all copies
    SCH_SHEET_LIST hierarchy = m_frame->Schematic().BuildSheetListSortedByPageNumbers();
    SCH_SHEET_LIST newInstances =
            hierarchy.FindAllSheetsForScreen( m_frame->GetCurrentSheet().LastScreen() );
    newInstances.SortByPageNumbers();

    // Get a list of all references in the schematic to avoid duplicates wherever
    // they're placed
    SCH_REFERENCE_LIST existingRefs;
    hierarchy.GetSymbols( existingRefs );
    existingRefs.SortByReferenceOnly();

    if( aEvent.IsAction( &EE_ACTIONS::placeSymbol ) )
    {
        historyList = &m_symbolHistoryList;
    }
    else if (aEvent.IsAction( &EE_ACTIONS::placePower ) )
    {
        historyList = &m_powerHistoryList;
        filter.FilterPowerSymbols( true );
    }
    else
    {
        wxFAIL_MSG( "PlaceSymbol(): unexpected request" );
    }

    m_frame->PushTool( aEvent );

    auto addSymbol =
            [this]( SCH_SYMBOL* aSymbol )
            {
                m_toolMgr->RunAction( EE_ACTIONS::clearSelection );
                m_selectionTool->AddItemToSel( aSymbol );

                aSymbol->SetFlags( IS_NEW | IS_MOVING );

                m_view->ClearPreview();
                m_view->AddToPreview( aSymbol, false );   // Add, but not give ownership

                // Set IS_MOVING again, as AddItemToCommitAndScreen() will have cleared it.
                aSymbol->SetFlags( IS_MOVING );
                m_toolMgr->PostAction( ACTIONS::refreshPreview );
            };

    auto setCursor =
            [&]()
            {
                m_frame->GetCanvas()->SetCurrentCursor( symbol ? KICURSOR::MOVING
                                                               : KICURSOR::COMPONENT );
            };

    auto cleanup =
            [&]()
            {
                m_toolMgr->RunAction( EE_ACTIONS::clearSelection );
                m_view->ClearPreview();
                delete symbol;
                symbol = nullptr;

                existingRefs.Clear();
                hierarchy.GetSymbols( existingRefs );
                existingRefs.SortByReferenceOnly();
            };

    auto annotate =
            [&]()
            {
                EESCHEMA_SETTINGS* cfg = m_frame->eeconfig();

                // Then we need to annotate all instances by sheet
                for( SCH_SHEET_PATH& instance : newInstances )
                {
                    SCH_REFERENCE newReference( symbol, instance );
                    SCH_REFERENCE_LIST refs;
                    refs.AddItem( newReference );

                    if( cfg->m_AnnotatePanel.automatic || newReference.AlwaysAnnotate() )
                    {
                        refs.ReannotateByOptions( (ANNOTATE_ORDER_T) cfg->m_AnnotatePanel.sort_order,
                                                  (ANNOTATE_ALGO_T) cfg->m_AnnotatePanel.method,
                                                  m_frame->Schematic().Settings().m_AnnotateStartNum,
                                                  existingRefs, false, &hierarchy );

                        refs.UpdateAnnotation();

                        // Update existing refs for next iteration
                        for( size_t i = 0; i < refs.GetCount(); i++ )
                            existingRefs.AddItem( refs[i] );
                    }
                }

                m_frame->GetCurrentSheet().UpdateAllScreenReferences();
            };

    Activate();

    // Must be done after Activate() so that it gets set into the correct context
    getViewControls()->ShowCursor( true );

    // Set initial cursor
    setCursor();

    // Prime the pump
    if( symbol )
    {
        addSymbol( symbol );
        annotate();
        getViewControls()->WarpMouseCursor( getViewControls()->GetMousePosition( false ) );
    }
    else if( aEvent.HasPosition() )
    {
        m_toolMgr->PrimeTool( aEvent.Position() );
    }
    else if( common_settings->m_Input.immediate_actions && !aEvent.IsReactivate() )
    {
        m_toolMgr->PrimeTool( { 0, 0 } );
        ignorePrimePosition = true;
    }

    // Main loop: keep receiving events
    while( TOOL_EVENT* evt = Wait() )
    {
        setCursor();
        grid.SetSnap( !evt->Modifier( MD_SHIFT ) );
        grid.SetUseGrid( getView()->GetGAL()->GetGridSnapping() && !evt->DisableGridSnapping() );

        cursorPos = grid.Align( controls->GetMousePosition(), GRID_HELPER_GRIDS::GRID_CONNECTABLE );
        controls->ForceCursorPosition( true, cursorPos );

        // The tool hotkey is interpreted as a click when drawing
        bool isSyntheticClick = symbol && evt->IsActivate() && evt->HasPosition()
                                && evt->Matches( aEvent );

        if( evt->IsCancelInteractive() || ( symbol && evt->IsAction( &ACTIONS::undo ) ) )
        {
            m_frame->GetInfoBar()->Dismiss();

            if( symbol )
            {
                cleanup();
            }
            else
            {
                m_frame->PopTool( aEvent );
                break;
            }
        }
        else if( evt->IsActivate() && !isSyntheticClick )
        {
            if( symbol && evt->IsMoveTool() )
            {
                // we're already moving our own item; ignore the move tool
                evt->SetPassEvent( false );
                continue;
            }

            if( symbol )
            {
                m_frame->ShowInfoBarMsg( _( "Press <ESC> to cancel symbol creation." ) );
                evt->SetPassEvent( false );
                continue;
            }

            if( evt->IsMoveTool() )
            {
                // leave ourselves on the stack so we come back after the move
                break;
            }
            else
            {
                m_frame->PopTool( aEvent );
                break;
            }
        }
        else if( evt->IsClick( BUT_LEFT ) || evt->IsDblClick( BUT_LEFT ) || isSyntheticClick )
        {
            if( !symbol )
            {
                m_toolMgr->RunAction( EE_ACTIONS::clearSelection );

                SYMBOL_LIB_TABLE* libs = PROJECT_SCH::SchSymbolLibTable( &m_frame->Prj() );
                SYMBOL_LIB*       cache = PROJECT_SCH::SchLibs( &m_frame->Prj() )->GetCacheLibrary();

                auto compareByLibID =
                        []( const LIB_SYMBOL* aFirst, const LIB_SYMBOL* aSecond ) -> bool
                        {
                            return aFirst->GetLibId().Format() < aSecond->GetLibId().Format();
                        };

                std::set<LIB_SYMBOL*, decltype( compareByLibID )> part_list( compareByLibID );

                for( SCH_SHEET_PATH& sheet : hierarchy )
                {
                    for( SCH_ITEM* item : sheet.LastScreen()->Items().OfType( SCH_SYMBOL_T ) )
                    {
                        SCH_SYMBOL* s = static_cast<SCH_SYMBOL*>( item );
                        LIB_SYMBOL* libSymbol = SchGetLibSymbol( s->GetLibId(), libs, cache );

                        if( libSymbol )
                            part_list.insert( libSymbol );
                    }
                }

                std::vector<PICKED_SYMBOL> alreadyPlaced;

                for( LIB_SYMBOL* libSymbol : part_list )
                {
                    PICKED_SYMBOL pickedSymbol;
                    pickedSymbol.LibId = libSymbol->GetLibId();
                    alreadyPlaced.push_back( pickedSymbol );
                }

                // Pick the symbol to be placed
                bool footprintPreviews = m_frame->eeconfig()->m_Appearance.footprint_preview;
                PICKED_SYMBOL sel = m_frame->PickSymbolFromLibrary( &filter, *historyList,
                                                                    alreadyPlaced,
                                                                    footprintPreviews );

                LIB_SYMBOL* libSymbol = sel.LibId.IsValid() ? m_frame->GetLibSymbol( sel.LibId )
                                                            : nullptr;

                if( !libSymbol )
                    continue;

                // If we started with a hotkey which has a position then warp back to that.
                // Otherwise update to the current mouse position pinned inside the autoscroll
                // boundaries.
                if( evt->IsPrime() && !ignorePrimePosition )
                {
                    cursorPos = grid.Align( evt->Position(), GRID_HELPER_GRIDS::GRID_CONNECTABLE );
                    getViewControls()->WarpMouseCursor( cursorPos, true );
                }
                else
                {
                    getViewControls()->PinCursorInsideNonAutoscrollArea( true );
                    cursorPos = grid.Align( getViewControls()->GetMousePosition(),
                                            GRID_HELPER_GRIDS::GRID_CONNECTABLE );
                }

                symbol = new SCH_SYMBOL( *libSymbol, &m_frame->GetCurrentSheet(), sel, cursorPos,
                                         &m_frame->Schematic() );
                addSymbol( symbol );
                annotate();

                // Update the list of references for the next symbol placement.
                SCH_REFERENCE placedSymbolReference( symbol, m_frame->GetCurrentSheet() );
                existingRefs.AddItem( placedSymbolReference );
                existingRefs.SortByReferenceOnly();

                if( m_frame->eeconfig()->m_AutoplaceFields.enable )
                    symbol->AutoplaceFields( /* aScreen */ nullptr, /* aManual */ false );

                // Update cursor now that we have a symbol
                setCursor();
            }
            else
            {
                m_view->ClearPreview();
                m_frame->AddToScreen( symbol, screen );

                if( m_frame->eeconfig()->m_AutoplaceFields.enable )
                    symbol->AutoplaceFields( screen, false /* aManual */ );

                m_frame->SaveCopyForRepeatItem( symbol );

                SCH_COMMIT commit( m_toolMgr );
                commit.Added( symbol, screen );

                SCH_LINE_WIRE_BUS_TOOL* lwbTool = m_toolMgr->GetTool<SCH_LINE_WIRE_BUS_TOOL>();
                lwbTool->TrimOverLappingWires( &commit, &m_selectionTool->GetSelection() );
                lwbTool->AddJunctionsIfNeeded( &commit, &m_selectionTool->GetSelection() );

                commit.Push( _( "Add Symbol" ) );

                SCH_SYMBOL* nextSymbol = nullptr;

                if( m_frame->eeconfig()->m_SymChooserPanel.place_all_units
                        || m_frame->eeconfig()->m_SymChooserPanel.keep_symbol )
                {
                    int new_unit = symbol->GetUnit();

                    if( m_frame->eeconfig()->m_SymChooserPanel.place_all_units
                        && symbol->GetUnit() < symbol->GetUnitCount() )
                    {
                        new_unit++;
                    }
                    else
                    {
                        new_unit = 1;
                    }

                    // We are either stepping to the next unit or next symbol
                    if( m_frame->eeconfig()->m_SymChooserPanel.keep_symbol || new_unit > 1 )
                    {
                        nextSymbol = static_cast<SCH_SYMBOL*>( symbol->Duplicate() );
                        nextSymbol->SetUnit( new_unit );
                        nextSymbol->SetUnitSelection( new_unit );

                        // Start new annotation sequence at first unit
                        if( new_unit == 1 )
                            nextSymbol->ClearAnnotation( nullptr, false );

                        addSymbol( nextSymbol );
                        symbol = nextSymbol;
                        annotate();

                        // Update the list of references for the next symbol placement.
                        SCH_REFERENCE placedSymbolReference( symbol, m_frame->GetCurrentSheet() );
                        existingRefs.AddItem( placedSymbolReference );
                        existingRefs.SortByReferenceOnly();
                    }
                }

                symbol = nextSymbol;
            }
        }
        else if( evt->IsClick( BUT_RIGHT ) )
        {
            // Warp after context menu only if dragging...
            if( !symbol )
                m_toolMgr->VetoContextMenuMouseWarp();

            m_menu.ShowContextMenu( m_selectionTool->GetSelection() );
        }
        else if( evt->Category() == TC_COMMAND && evt->Action() == TA_CHOICE_MENU_CHOICE )
        {
            if( *evt->GetCommandId() >= ID_POPUP_SCH_SELECT_UNIT
                && *evt->GetCommandId() <= ID_POPUP_SCH_SELECT_UNIT_END )
            {
                int unit = *evt->GetCommandId() - ID_POPUP_SCH_SELECT_UNIT;

                if( symbol )
                {
                    m_frame->SelectUnit( symbol, unit );
                    m_toolMgr->PostAction( ACTIONS::refreshPreview );
                }
            }
            else if( *evt->GetCommandId() >= ID_POPUP_SCH_SELECT_BASE
                     && *evt->GetCommandId() <= ID_POPUP_SCH_SELECT_ALT )
            {
                int bodyStyle = ( *evt->GetCommandId() - ID_POPUP_SCH_SELECT_BASE ) + 1;

                if( symbol && symbol->GetBodyStyle() != bodyStyle )
                {
                    m_frame->FlipBodyStyle( symbol );
                    m_toolMgr->PostAction( ACTIONS::refreshPreview );
                }
            }
        }
        else if( evt->IsAction( &ACTIONS::duplicate )
                 || evt->IsAction( &EE_ACTIONS::repeatDrawItem ) )
        {
            if( symbol )
            {
                // This doesn't really make sense; we'll just end up dragging a stack of
                // objects so we ignore the duplicate and just carry on.
                wxBell();
                continue;
            }

            // Exit.  The duplicate will run in its own loop.
            m_frame->PopTool( aEvent );
            break;
        }
        else if( symbol && ( evt->IsAction( &ACTIONS::refreshPreview ) || evt->IsMotion() ) )
        {
            symbol->SetPosition( cursorPos );
            m_view->ClearPreview();
            m_view->AddToPreview( symbol, false );   // Add, but not give ownership
            m_frame->SetMsgPanel( symbol );
        }
        else if( symbol && evt->IsAction( &ACTIONS::doDelete ) )
        {
            cleanup();
        }
        else if( symbol && evt->IsAction( &ACTIONS::redo ) )
        {
            wxBell();
        }
        else
        {
            evt->SetPassEvent();
        }

        // Enable autopanning and cursor capture only when there is a symbol to be placed
        getViewControls()->SetAutoPan( symbol != nullptr );
        getViewControls()->CaptureCursor( symbol != nullptr );
    }

    getViewControls()->SetAutoPan( false );
    getViewControls()->CaptureCursor( false );
    m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::ARROW );

    return 0;
}


int SCH_DRAWING_TOOLS::PlaceImage( const TOOL_EVENT& aEvent )
{
    SCH_BITMAP*      image = aEvent.Parameter<SCH_BITMAP*>();
    bool             immediateMode = image != nullptr;
    bool             ignorePrimePosition = false;
    COMMON_SETTINGS* common_settings = Pgm().GetCommonSettings();

    if( m_inDrawingTool )
        return 0;

    REENTRANCY_GUARD guard( &m_inDrawingTool );

    EE_GRID_HELPER        grid( m_toolMgr );
    KIGFX::VIEW_CONTROLS* controls = getViewControls();
    VECTOR2I              cursorPos;

    m_toolMgr->RunAction( EE_ACTIONS::clearSelection );

    // Add all the drawable symbols to preview
    if( image )
    {
        image->SetPosition( getViewControls()->GetCursorPosition() );
        m_view->ClearPreview();
        m_view->AddToPreview( image, false );   // Add, but not give ownership
    }

    m_frame->PushTool( aEvent );

    auto setCursor =
            [&]()
            {
                if( image )
                    m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::MOVING );
                else
                    m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::PENCIL );
            };

    auto cleanup =
            [&] ()
            {
                m_toolMgr->RunAction( EE_ACTIONS::clearSelection );
                m_view->ClearPreview();
                m_view->RecacheAllItems();
                delete image;
                image = nullptr;
            };

    Activate();

    // Must be done after Activate() so that it gets set into the correct context
    getViewControls()->ShowCursor( true );

    // Set initial cursor
    setCursor();

    // Prime the pump
    if( image )
    {
        m_toolMgr->PostAction( ACTIONS::refreshPreview );
    }
    else if( aEvent.HasPosition() )
    {
        m_toolMgr->PrimeTool( aEvent.Position() );
    }
    else if( common_settings->m_Input.immediate_actions && !aEvent.IsReactivate() )
    {
        m_toolMgr->PrimeTool( { 0, 0 } );
        ignorePrimePosition = true;
    }

    // Main loop: keep receiving events
    while( TOOL_EVENT* evt = Wait() )
    {
        setCursor();
        grid.SetSnap( !evt->Modifier( MD_SHIFT ) );
        grid.SetUseGrid( getView()->GetGAL()->GetGridSnapping() && !evt->DisableGridSnapping() );

        cursorPos = grid.Align( controls->GetMousePosition(), GRID_HELPER_GRIDS::GRID_GRAPHICS );
        controls->ForceCursorPosition( true, cursorPos );

        // The tool hotkey is interpreted as a click when drawing
        bool isSyntheticClick = image && evt->IsActivate() && evt->HasPosition()
                                && evt->Matches( aEvent );

        if( evt->IsCancelInteractive() || ( image && evt->IsAction( &ACTIONS::undo ) ) )
        {
            m_frame->GetInfoBar()->Dismiss();

            if( image )
            {
                cleanup();
            }
            else
            {
                m_frame->PopTool( aEvent );
                break;
            }

            if( immediateMode )
            {
                m_frame->PopTool( aEvent );
                break;
            }
        }
        else if( evt->IsActivate() && !isSyntheticClick )
        {
            if( image && evt->IsMoveTool() )
            {
                // we're already moving our own item; ignore the move tool
                evt->SetPassEvent( false );
                continue;
            }

            if( image )
            {
                m_frame->ShowInfoBarMsg( _( "Press <ESC> to cancel image creation." ) );
                evt->SetPassEvent( false );
                continue;
            }

            if( evt->IsMoveTool() )
            {
                // leave ourselves on the stack so we come back after the move
                break;
            }
            else
            {
                m_frame->PopTool( aEvent );
                break;
            }
        }
        else if( evt->IsClick( BUT_LEFT ) || evt->IsDblClick( BUT_LEFT ) || isSyntheticClick )
        {
            if( !image )
            {
                m_toolMgr->RunAction( EE_ACTIONS::clearSelection );

                wxFileDialog dlg( m_frame, _( "Choose Image" ), m_mruPath, wxEmptyString,
                                  _( "Image Files" ) + wxS( " " ) + wxImage::GetImageExtWildcard(),
                                  wxFD_OPEN );

                if( dlg.ShowModal() != wxID_OK )
                    continue;

                // If we started with a hotkey which has a position then warp back to that.
                // Otherwise update to the current mouse position pinned inside the autoscroll
                // boundaries.
                if( evt->IsPrime() && !ignorePrimePosition )
                {
                    cursorPos = grid.Align( evt->Position() );
                    getViewControls()->WarpMouseCursor( cursorPos, true );
                }
                else
                {
                    getViewControls()->PinCursorInsideNonAutoscrollArea( true );
                    cursorPos = getViewControls()->GetMousePosition();
                }

                wxString fullFilename = dlg.GetPath();
                m_mruPath = wxPathOnly( fullFilename );

                if( wxFileExists( fullFilename ) )
                    image = new SCH_BITMAP( cursorPos );

                if( !image || !image->ReadImageFile( fullFilename ) )
                {
                    wxMessageBox( _( "Could not load image from '%s'." ), fullFilename );
                    delete image;
                    image = nullptr;
                    continue;
                }

                image->SetFlags( IS_NEW | IS_MOVING );

                m_frame->SaveCopyForRepeatItem( image );

                m_view->ClearPreview();
                m_view->AddToPreview( image, false );   // Add, but not give ownership
                m_view->RecacheAllItems();              // Bitmaps are cached in Opengl

                m_selectionTool->AddItemToSel( image );

                getViewControls()->SetCursorPosition( cursorPos, false );
                setCursor();
            }
            else
            {
                SCH_COMMIT commit( m_toolMgr );
                commit.Add( image, m_frame->GetScreen() );
                commit.Push( _( "Add Image" ) );

                image = nullptr;
                m_toolMgr->PostAction( ACTIONS::activatePointEditor );

                m_view->ClearPreview();

                if( immediateMode )
                {
                    m_frame->PopTool( aEvent );
                    break;
                }
            }
        }
        else if( evt->IsClick( BUT_RIGHT ) )
        {
            // Warp after context menu only if dragging...
            if( !image )
                m_toolMgr->VetoContextMenuMouseWarp();

            m_menu.ShowContextMenu( m_selectionTool->GetSelection() );
        }
        else if( evt->IsAction( &ACTIONS::duplicate )
                 || evt->IsAction( &EE_ACTIONS::repeatDrawItem ) )
        {
            if( image )
            {
                // This doesn't really make sense; we'll just end up dragging a stack of
                // objects so we ignore the duplicate and just carry on.
                wxBell();
                continue;
            }

            // Exit.  The duplicate will run in its own loop.
            m_frame->PopTool( aEvent );
            break;
        }
        else if( image && ( evt->IsAction( &ACTIONS::refreshPreview ) || evt->IsMotion() ) )
        {
            image->SetPosition( cursorPos );
            m_view->ClearPreview();
            m_view->AddToPreview( image, false );   // Add, but not give ownership
            m_view->RecacheAllItems();              // Bitmaps are cached in Opengl
            m_frame->SetMsgPanel( image );
        }
        else if( image && evt->IsAction( &ACTIONS::doDelete ) )
        {
            cleanup();
        }
        else if( image && evt->IsAction( &ACTIONS::redo ) )
        {
            wxBell();
        }
        else
        {
            evt->SetPassEvent();
        }

        // Enable autopanning and cursor capture only when there is an image to be placed
        getViewControls()->SetAutoPan( image != nullptr );
        getViewControls()->CaptureCursor( image != nullptr );
    }

    getViewControls()->SetAutoPan( false );
    getViewControls()->CaptureCursor( false );
    m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::ARROW );

    return 0;
}


int SCH_DRAWING_TOOLS::ImportGraphics( const TOOL_EVENT& aEvent )
{
    if( m_inDrawingTool )
        return 0;

    REENTRANCY_GUARD guard( &m_inDrawingTool );

    // Note: PlaceImportedGraphics() will convert PCB_SHAPE_T and PCB_TEXT_T to footprint
    // items if needed
    DIALOG_IMPORT_GFX_SCH dlg( m_frame );
    int                   dlgResult = dlg.ShowModal();

    std::list<std::unique_ptr<EDA_ITEM>>& list = dlg.GetImportedItems();

    if( dlgResult != wxID_OK )
        return 0;

    // Ensure the list is not empty:
    if( list.empty() )
    {
        wxMessageBox( _( "No graphic items found in file." ) );
        return 0;
    }

    m_toolMgr->RunAction( ACTIONS::cancelInteractive );

    KIGFX::VIEW_CONTROLS*  controls = getViewControls();
    std::vector<SCH_ITEM*> newItems;      // all new items, including group
    std::vector<SCH_ITEM*> selectedItems; // the group, or newItems if no group
    EE_SELECTION           preview;
    SCH_COMMIT             commit( m_toolMgr );

    for( std::unique_ptr<EDA_ITEM>& ptr : list )
    {
        SCH_ITEM* item = dynamic_cast<SCH_ITEM*>( ptr.get() );
        wxCHECK2( item, continue );

        newItems.push_back( item );
        selectedItems.push_back( item );
        preview.Add( item );

        ptr.release();
    }

    if( !dlg.IsPlacementInteractive() )
    {
        // Place the imported drawings
        for( SCH_ITEM* item : newItems )
            commit.Add(item, m_frame->GetScreen());

        commit.Push( _( "Import Graphic" ) );
        return 0;
    }

    m_view->Add( &preview );

    // Clear the current selection then select the drawings so that edit tools work on them
    m_toolMgr->RunAction( EE_ACTIONS::clearSelection );

    EDA_ITEMS selItems( selectedItems.begin(), selectedItems.end() );
    m_toolMgr->RunAction<EDA_ITEMS*>( EE_ACTIONS::addItemsToSel, &selItems );

    m_frame->PushTool( aEvent );

    auto setCursor = [&]()
    {
        m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::MOVING );
    };

    Activate();
    // Must be done after Activate() so that it gets set into the correct context
    controls->ShowCursor( true );
    controls->ForceCursorPosition( false );
    // Set initial cursor
    setCursor();

    //SCOPED_DRAW_MODE scopedDrawMode( m_mode, MODE::DXF );
    EE_GRID_HELPER grid( m_toolMgr );

    // Now move the new items to the current cursor position:
    VECTOR2I cursorPos = controls->GetCursorPosition( !aEvent.DisableGridSnapping() );
    VECTOR2I delta = cursorPos;
    VECTOR2I currentOffset;

    for( SCH_ITEM* item : selectedItems )
        item->Move( delta );

    currentOffset += delta;

    m_view->Update( &preview );

    // Main loop: keep receiving events
    while( TOOL_EVENT* evt = Wait() )
    {
        setCursor();

        grid.SetSnap( !evt->Modifier( MD_SHIFT ) );
        grid.SetUseGrid( getView()->GetGAL()->GetGridSnapping() && !evt->DisableGridSnapping() );

        cursorPos = grid.Align( controls->GetMousePosition(), GRID_GRAPHICS );
        controls->ForceCursorPosition( true, cursorPos );

        if( evt->IsCancelInteractive() || evt->IsActivate() )
        {
            m_toolMgr->RunAction( EE_ACTIONS::clearSelection );

            for( SCH_ITEM* item : newItems )
                delete item;

            break;
        }
        else if( evt->IsMotion() )
        {
            delta = cursorPos - currentOffset;

            for( SCH_ITEM* item : selectedItems )
                item->Move( delta );

            currentOffset += delta;

            m_view->Update( &preview );
        }
        else if( evt->IsClick( BUT_RIGHT ) )
        {
            m_menu.ShowContextMenu( m_selectionTool->GetSelection() );
        }
        else if( evt->IsClick( BUT_LEFT ) || evt->IsDblClick( BUT_LEFT ) )
        {
            // Place the imported drawings
            for( SCH_ITEM* item : newItems )
                commit.Add( item, m_frame->GetScreen() );

            commit.Push( _( "Import Graphic" ) );
            break; // This is a one-shot command, not a tool
        }
        else
        {
            evt->SetPassEvent();
        }
    }

    preview.Clear();
    m_view->Remove( &preview );

    m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::ARROW );
    controls->ForceCursorPosition( false );

    m_frame->PopTool( aEvent );

    return 0;
}


int SCH_DRAWING_TOOLS::SingleClickPlace( const TOOL_EVENT& aEvent )
{
    VECTOR2I              cursorPos;
    KICAD_T               type = aEvent.Parameter<KICAD_T>();
    EE_GRID_HELPER        grid( m_toolMgr );
    KIGFX::VIEW_CONTROLS* controls = getViewControls();
    SCH_ITEM*             previewItem;
    bool                  loggedInfoBarError = false;
    wxString              description;
    SCH_SCREEN*           screen = m_frame->GetScreen();
    bool                  allowRepeat = false;  // Set to true to allow new item repetition

    if( m_inDrawingTool )
        return 0;

    REENTRANCY_GUARD guard( &m_inDrawingTool );

    if( type == SCH_JUNCTION_T && aEvent.HasPosition() )
    {
        EE_SELECTION& selection = m_selectionTool->GetSelection();
        SCH_LINE*     wire = dynamic_cast<SCH_LINE*>( selection.Front() );

        if( wire )
        {
            SEG seg( wire->GetStartPoint(), wire->GetEndPoint() );
            VECTOR2I nearest = seg.NearestPoint( getViewControls()->GetCursorPosition() );
            getViewControls()->SetCrossHairCursorPosition( nearest, false );
            getViewControls()->WarpMouseCursor( getViewControls()->GetCursorPosition(), true );
        }
    }

    switch( type )
    {
    case SCH_NO_CONNECT_T:
        previewItem = new SCH_NO_CONNECT( cursorPos );
        previewItem->SetParent( screen );
        description = _( "Add No Connect Flag" );
        allowRepeat = true;
        break;

    case SCH_JUNCTION_T:
        previewItem = new SCH_JUNCTION( cursorPos );
        previewItem->SetParent( screen );
        description = _( "Add Junction" );
        break;

    case SCH_BUS_WIRE_ENTRY_T:
        previewItem = new SCH_BUS_WIRE_ENTRY( cursorPos );
        previewItem->SetParent( screen );
        description = _( "Add Wire to Bus Entry" );
        allowRepeat = true;
        break;

    default:
        wxASSERT_MSG( false, "Unknown item type in SCH_DRAWING_TOOLS::SingleClickPlace" );
        return 0;
    }

    m_toolMgr->RunAction( EE_ACTIONS::clearSelection );

    cursorPos = aEvent.HasPosition() ? aEvent.Position() : controls->GetMousePosition();

    m_frame->PushTool( aEvent );

    auto setCursor =
            [&]()
            {
                m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::PLACE );
            };

    Activate();

    // Must be done after Activate() so that it gets set into the correct context
    getViewControls()->ShowCursor( true );

    // Set initial cursor
    setCursor();

    m_view->ClearPreview();
    m_view->AddToPreview( previewItem->Clone() );

    // Prime the pump
    if( aEvent.HasPosition() && type != SCH_SHEET_PIN_T )
        m_toolMgr->PrimeTool( aEvent.Position() );
    else
        m_toolMgr->PostAction( ACTIONS::refreshPreview );

    // Main loop: keep receiving events
    while( TOOL_EVENT* evt = Wait() )
    {
        setCursor();
        grid.SetSnap( !evt->Modifier( MD_SHIFT ) );
        grid.SetUseGrid( getView()->GetGAL()->GetGridSnapping() && !evt->DisableGridSnapping() );

        cursorPos = evt->IsPrime() ? evt->Position() : controls->GetMousePosition();
        cursorPos = grid.BestSnapAnchor( cursorPos, grid.GetItemGrid( previewItem ), nullptr );
        controls->ForceCursorPosition( true, cursorPos );

        if( evt->IsCancelInteractive() )
        {
            m_frame->PopTool( aEvent );
            break;
        }
        else if( evt->IsActivate() )
        {
            if( evt->IsMoveTool() )
            {
                // leave ourselves on the stack so we come back after the move
                break;
            }
            else
            {
                m_frame->PopTool( aEvent );
                break;
            }
        }
        else if( evt->IsClick( BUT_LEFT ) || evt->IsDblClick( BUT_LEFT ) )
        {
            if( !screen->GetItem( cursorPos, 0, type ) )
            {
                if( type == SCH_JUNCTION_T )
                {
                    if( !screen->IsExplicitJunctionAllowed( cursorPos ) )
                    {
                        m_frame->ShowInfoBarError( _( "Junction location contains no joinable "
                                                      "wires and/or pins." ) );
                        loggedInfoBarError = true;
                        continue;
                    }
                    else if( loggedInfoBarError )
                    {
                        m_frame->GetInfoBar()->Dismiss();
                    }
                }

                SCH_ITEM* newItem = static_cast<SCH_ITEM*>( previewItem->Clone() );
                const_cast<KIID&>( newItem->m_Uuid ) = KIID();
                newItem->SetPosition( cursorPos );
                newItem->SetFlags( IS_NEW );
                m_frame->AddToScreen( newItem, screen );

                if( allowRepeat )
                    m_frame->SaveCopyForRepeatItem( newItem );

                SCH_COMMIT commit( m_toolMgr );
                commit.Added( newItem, screen );

                m_frame->SchematicCleanUp( &commit );

                commit.Push( description );
            }

            if( evt->IsDblClick( BUT_LEFT ) || type == SCH_SHEET_PIN_T )  // Finish tool.
            {
                m_frame->PopTool( aEvent );
                break;
            }
        }
        else if( evt->IsClick( BUT_RIGHT ) )
        {
            m_menu.ShowContextMenu( m_selectionTool->GetSelection() );
        }
        else if( evt->IsAction( &ACTIONS::refreshPreview ) || evt->IsMotion() )
        {
            previewItem->SetPosition( cursorPos );
            m_view->ClearPreview();
            m_view->AddToPreview( previewItem->Clone() );
            m_frame->SetMsgPanel( previewItem );
        }
        else if( evt->Category() == TC_COMMAND )
        {
            if( ( type == SCH_BUS_WIRE_ENTRY_T ) && (   evt->IsAction( &EE_ACTIONS::rotateCW )
                                                     || evt->IsAction( &EE_ACTIONS::rotateCCW )
                                                     || evt->IsAction( &EE_ACTIONS::mirrorV )
                                                     || evt->IsAction( &EE_ACTIONS::mirrorH ) ) )
            {
                SCH_BUS_ENTRY_BASE* busItem = static_cast<SCH_BUS_ENTRY_BASE*>( previewItem );

                if( evt->IsAction( &EE_ACTIONS::rotateCW ) )
                {
                    busItem->Rotate( busItem->GetPosition(), false );
                }
                else if( evt->IsAction( &EE_ACTIONS::rotateCCW ) )
                {
                    busItem->Rotate( busItem->GetPosition(), true );
                }
                else if( evt->IsAction( &EE_ACTIONS::mirrorV ) )
                {
                    busItem->MirrorVertically( busItem->GetPosition().y );
                }
                else if( evt->IsAction( &EE_ACTIONS::mirrorH ) )
                {
                    busItem->MirrorHorizontally( busItem->GetPosition().x );
                }

                m_view->ClearPreview();
                m_view->AddToPreview( previewItem->Clone() );
            }
            else if( evt->IsAction( &EE_ACTIONS::properties ) )
            {
                switch( type )
                {
                case SCH_BUS_WIRE_ENTRY_T:
                {
                    std::deque<SCH_ITEM*> strokeItems;
                    strokeItems.push_back( previewItem );

                    DIALOG_WIRE_BUS_PROPERTIES dlg( m_frame, strokeItems );
                }
                    break;

                case SCH_JUNCTION_T:
                {
                    std::deque<SCH_JUNCTION*> junctions;
                    junctions.push_back( static_cast<SCH_JUNCTION*>( previewItem ) );

                    DIALOG_JUNCTION_PROPS dlg( m_frame, junctions );
                }
                    break;
                default:
                    // Do nothing
                    break;
                }

                m_view->ClearPreview();
                m_view->AddToPreview( previewItem->Clone() );
            }
            else
            {
                evt->SetPassEvent();
            }
        }
        else
        {
            evt->SetPassEvent();
        }
    }

    delete previewItem;
    m_view->ClearPreview();

    m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::ARROW );
    controls->ForceCursorPosition( false );

    return 0;
}


SCH_LINE* SCH_DRAWING_TOOLS::findWire( const VECTOR2I& aPosition )
{
    for( SCH_ITEM* item : m_frame->GetScreen()->Items().Overlapping( SCH_LINE_T, aPosition ) )
    {
        SCH_LINE* line = static_cast<SCH_LINE*>( item );

        if( line->GetEditFlags() & STRUCT_DELETED )
            continue;

        if( line->IsWire() )
            return line;
    }

    return nullptr;
}


wxString SCH_DRAWING_TOOLS::findWireLabelDriverName( SCH_LINE* aWire )
{
    wxASSERT( aWire->IsWire() );

    SCH_SHEET_PATH sheetPath = m_frame->GetCurrentSheet();

    if( SCH_CONNECTION* wireConnection = aWire->Connection( &sheetPath ) )
    {
        SCH_ITEM* wireDriver = wireConnection->Driver();

        if( wireDriver && wireDriver->IsType( { SCH_LABEL_T, SCH_GLOBAL_LABEL_T } ) )
            return wireConnection->LocalName();
    }

    return wxEmptyString;
}


SCH_TEXT* SCH_DRAWING_TOOLS::createNewText( const VECTOR2I& aPosition, int aType )
{
    SCHEMATIC*          schematic = getModel<SCHEMATIC>();
    SCHEMATIC_SETTINGS& settings = schematic->Settings();
    SCH_TEXT*           textItem = nullptr;
    SCH_LABEL_BASE*     labelItem = nullptr;
    wxString            netName;

    switch( aType )
    {
    case LAYER_NOTES:
        textItem = new SCH_TEXT( aPosition );
        break;

    case LAYER_LOCLABEL:
        labelItem = new SCH_LABEL( aPosition );
        textItem = labelItem;

        if( SCH_LINE* wire = findWire( aPosition ) )
            netName = findWireLabelDriverName( wire );

        break;

    case LAYER_NETCLASS_REFS:
        labelItem = new SCH_DIRECTIVE_LABEL( aPosition );
        labelItem->SetShape( m_lastNetClassFlagShape );
        labelItem->GetFields().emplace_back( SCH_FIELD( {0,0}, 0, labelItem, wxT( "Netclass" ) ) );
        labelItem->GetFields().back().SetItalic( true );
        labelItem->GetFields().back().SetVisible( true );
        textItem = labelItem;
        break;

    case LAYER_HIERLABEL:
        labelItem = new SCH_HIERLABEL( aPosition );
        labelItem->SetShape( m_lastGlobalLabelShape );
        labelItem->SetAutoRotateOnPlacement( m_lastAutoLabelRotateOnPlacement );
        textItem = labelItem;
        break;

    case LAYER_GLOBLABEL:
        labelItem = new SCH_GLOBALLABEL( aPosition );
        labelItem->SetShape( m_lastGlobalLabelShape );
        labelItem->GetFields()[0].SetVisible( settings.m_IntersheetRefsShow );
        labelItem->SetAutoRotateOnPlacement( m_lastAutoLabelRotateOnPlacement );
        textItem = labelItem;

        if( SCH_LINE* wire = findWire( aPosition ) )
            netName = findWireLabelDriverName( wire );

        break;

    default:
        wxFAIL_MSG( "SCH_EDIT_FRAME::CreateNewText() unknown layer type" );
        return nullptr;
    }

    textItem->SetParent( schematic );

    textItem->SetTextSize( VECTOR2I( settings.m_DefaultTextSize, settings.m_DefaultTextSize ) );

    if( aType != LAYER_NETCLASS_REFS )
    {
        // Must be after SetTextSize()
        textItem->SetBold( m_lastTextBold );
        textItem->SetItalic( m_lastTextItalic );
    }

    if( labelItem )
    {
        labelItem->SetSpinStyle( m_lastTextOrientation );
    }
    else
    {
        textItem->SetHorizJustify( m_lastTextHJustify );
        textItem->SetVertJustify( m_lastTextVJustify );
        textItem->SetTextAngle( m_lastTextAngle );
    }

    textItem->SetFlags( IS_NEW | IS_MOVING );

    if( !labelItem )
    {
        DIALOG_TEXT_PROPERTIES dlg( m_frame, textItem );

        // QuasiModal required for syntax help and Scintilla auto-complete
        if( dlg.ShowQuasiModal() != wxID_OK )
        {
            delete textItem;
            return nullptr;
        }
    }
    else if( !netName.IsEmpty() )
    {
        // Auto-create from attached wire
        textItem->SetText( netName );
    }
    else
    {
        DIALOG_LABEL_PROPERTIES dlg( m_frame, static_cast<SCH_LABEL_BASE*>( textItem ) );

        // QuasiModal required for syntax help and Scintilla auto-complete
        if( dlg.ShowQuasiModal() != wxID_OK )
        {
            delete labelItem;
            return nullptr;
        }
    }

    wxString text = textItem->GetText();

    if( textItem->Type() != SCH_DIRECTIVE_LABEL_T && NoPrintableChars( text ) )
    {
        delete textItem;
        return nullptr;
    }

    if( aType != LAYER_NETCLASS_REFS )
    {
        m_lastTextBold = textItem->IsBold();
        m_lastTextItalic = textItem->IsItalic();
    }

    if( labelItem )
    {
        m_lastTextOrientation = labelItem->GetSpinStyle();
    }
    else
    {
        m_lastTextHJustify = textItem->GetHorizJustify();
        m_lastTextVJustify = textItem->GetVertJustify();
        m_lastTextAngle = textItem->GetTextAngle();
    }

    if( aType == LAYER_GLOBLABEL || aType == LAYER_HIERLABEL )
    {
        m_lastGlobalLabelShape = labelItem->GetShape();
        m_lastAutoLabelRotateOnPlacement = labelItem->AutoRotateOnPlacement();
    }
    else if( aType == LAYER_NETCLASS_REFS )
    {
        m_lastNetClassFlagShape = labelItem->GetShape();
    }

    return textItem;
}

SCH_SHEET_PIN* SCH_DRAWING_TOOLS::createNewSheetPin( SCH_SHEET* aSheet, const VECTOR2I& aPosition )
{
    SCHEMATIC_SETTINGS& settings = aSheet->Schematic()->Settings();
    SCH_SHEET_PIN*      pin = new SCH_SHEET_PIN( aSheet );

    pin->SetFlags( IS_NEW | IS_MOVING );
    pin->SetText( std::to_string( aSheet->GetPins().size() + 1 ) );
    pin->SetTextSize( VECTOR2I( settings.m_DefaultTextSize, settings.m_DefaultTextSize ) );
    pin->SetPosition( aPosition );
    pin->ClearSelected();

    m_lastSheetPinType = pin->GetShape();

    return pin;
}


int SCH_DRAWING_TOOLS::TwoClickPlace( const TOOL_EVENT& aEvent )
{
    SCH_ITEM*             item = nullptr;
    KIGFX::VIEW_CONTROLS* controls = getViewControls();
    EE_GRID_HELPER        grid( m_toolMgr );
    bool                  ignorePrimePosition = false;
    COMMON_SETTINGS*      common_settings = Pgm().GetCommonSettings();
    SCH_SHEET*            sheet = nullptr;
    wxString              description;

    if( m_inDrawingTool )
        return 0;

    REENTRANCY_GUARD guard( &m_inDrawingTool );

    bool isText        = aEvent.IsAction( &EE_ACTIONS::placeSchematicText );
    bool isGlobalLabel = aEvent.IsAction( &EE_ACTIONS::placeGlobalLabel );
    bool isHierLabel   = aEvent.IsAction( &EE_ACTIONS::placeHierLabel );
    bool isClassLabel  = aEvent.IsAction( &EE_ACTIONS::placeClassLabel );
    bool isNetLabel    = aEvent.IsAction( &EE_ACTIONS::placeLabel );
    bool isSheetPin    = aEvent.IsAction( &EE_ACTIONS::placeSheetPin );

    GRID_HELPER_GRIDS snapGrid = isText ? GRID_TEXT : GRID_CONNECTABLE;

    // If we have a selected sheet use it, otherwise try to get one under the cursor
    if( isSheetPin )
        sheet = dynamic_cast<SCH_SHEET*>( m_selectionTool->GetSelection().Front() );

    m_toolMgr->RunAction( EE_ACTIONS::clearSelection );

    m_frame->PushTool( aEvent );

    auto setCursor =
            [&]()
            {
                if( item )
                    m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::PLACE );
                else if( isText )
                    m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::TEXT );
                else if( isGlobalLabel )
                    m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::LABEL_GLOBAL );
                else if( isNetLabel )
                    m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::LABEL_NET );
                else if( isClassLabel )
                    m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::LABEL_NET );    // JEY TODO: netclass directive cursor
                else if( isHierLabel )
                    m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::LABEL_HIER );
                else
                    m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::PENCIL );
            };

    auto updatePreview =
            [&]()
            {
                m_view->ClearPreview();
                m_view->AddToPreview( item->Clone() );
                item->RunOnChildren( [&]( SCH_ITEM* aChild )
                                     {
                                         m_view->AddToPreview( aChild->Clone() );
                                     } );
                m_frame->SetMsgPanel( item );
            };

    auto cleanup =
            [&]()
            {
                m_toolMgr->RunAction( EE_ACTIONS::clearSelection );
                m_view->ClearPreview();
                delete item;
                item = nullptr;
            };

    Activate();

    // Must be done after Activate() so that it gets set into the correct context
    controls->ShowCursor( true );

    // Set initial cursor
    setCursor();

    if( aEvent.HasPosition() )
    {
        m_toolMgr->PrimeTool( aEvent.Position() );
    }
    else if( common_settings->m_Input.immediate_actions && !aEvent.IsReactivate()
                && ( isText || isGlobalLabel || isHierLabel || isClassLabel || isNetLabel ) )
    {
        m_toolMgr->PrimeTool( { 0, 0 } );
        ignorePrimePosition = true;
    }

    // Main loop: keep receiving events
    while( TOOL_EVENT* evt = Wait() )
    {
        setCursor();
        grid.SetSnap( !evt->Modifier( MD_SHIFT ) );
        grid.SetUseGrid( getView()->GetGAL()->GetGridSnapping() && !evt->DisableGridSnapping() );

        VECTOR2I cursorPos = controls->GetMousePosition();
        cursorPos = grid.BestSnapAnchor( cursorPos, snapGrid, item );
        controls->ForceCursorPosition( true, cursorPos );

        // The tool hotkey is interpreted as a click when drawing
        bool isSyntheticClick = item && evt->IsActivate() && evt->HasPosition()
                                && evt->Matches( aEvent );

        if( evt->IsCancelInteractive() || evt->IsAction( &ACTIONS::undo ) )
        {
            m_frame->GetInfoBar()->Dismiss();

            if( item )
            {
                cleanup();
            }
            else
            {
                m_frame->PopTool( aEvent );
                break;
            }
        }
        else if( evt->IsActivate() && !isSyntheticClick )
        {
            if( item && evt->IsMoveTool() )
            {
                // we're already moving our own item; ignore the move tool
                evt->SetPassEvent( false );
                continue;
            }

            if( item )
            {
                m_frame->ShowInfoBarMsg( _( "Press <ESC> to cancel item creation." ) );
                evt->SetPassEvent( false );
                continue;
            }

            if( evt->IsPointEditor() )
            {
                // don't exit (the point editor runs in the background)
            }
            else if( evt->IsMoveTool() )
            {
                // leave ourselves on the stack so we come back after the move
                break;
            }
            else
            {
                m_frame->PopTool( aEvent );
                break;
            }
        }
        else if( evt->IsClick( BUT_LEFT ) || evt->IsDblClick( BUT_LEFT ) || isSyntheticClick )
        {
            // First click creates...
            if( !item )
            {
                m_toolMgr->RunAction( EE_ACTIONS::clearSelection );

                if( isText )
                {
                    item = createNewText( cursorPos, LAYER_NOTES );
                    description = _( "Add Text" );
                }
                else if( isHierLabel )
                {
                    if( m_dialogSyncSheetPin && m_dialogSyncSheetPin->GetPlacementTemplate() )
                    {
                        auto pin = static_cast<SCH_HIERLABEL*>(
                                m_dialogSyncSheetPin->GetPlacementTemplate() );
                        SCH_HIERLABEL* label = new SCH_HIERLABEL( cursorPos );
                        SCHEMATIC*     schematic = getModel<SCHEMATIC>();
                        label->SetText( pin->GetText() );
                        label->SetShape( pin->GetShape() );
                        label->SetAutoRotateOnPlacement( m_lastAutoLabelRotateOnPlacement );
                        label->SetParent( schematic );
                        label->SetBold( m_lastTextBold );
                        label->SetItalic( m_lastTextItalic );
                        label->SetSpinStyle( m_lastTextOrientation );
                        label->SetTextSize( VECTOR2I( schematic->Settings().m_DefaultTextSize,
                                                      schematic->Settings().m_DefaultTextSize ) );
                        label->SetFlags( IS_NEW | IS_MOVING );
                        item = label;
                    }
                    else
                    {
                        item = createNewText( cursorPos, LAYER_HIERLABEL );
                    }

                    description = _( "Add Hierarchical Label" );
                }
                else if( isNetLabel )
                {
                    item = createNewText( cursorPos, LAYER_LOCLABEL );
                    description = _( "Add Label" );
                }
                else if( isGlobalLabel )
                {
                    item = createNewText( cursorPos, LAYER_GLOBLABEL );
                    description = _( "Add Label" );
                }
                else if( isClassLabel )
                {
                    item = createNewText( cursorPos, LAYER_NETCLASS_REFS );
                    description = _( "Add Label" );
                }
                else if( isSheetPin )
                {
                    EDA_ITEM* i = nullptr;

                    // If we didn't have a sheet selected, try to find one under the cursor
                    if( !sheet && m_selectionTool->SelectPoint( cursorPos, { SCH_SHEET_T }, &i ) )
                        sheet = dynamic_cast<SCH_SHEET*>( i );

                    if( !sheet )
                    {
                        m_statusPopup = std::make_unique<STATUS_TEXT_POPUP>( m_frame );
                        m_statusPopup->SetText( _( "Click over a sheet." ) );
                        m_statusPopup->Move( KIPLATFORM::UI::GetMousePosition()
                                             + wxPoint( 20, 20 ) );
                        m_statusPopup->PopupFor( 2000 );
                        item = nullptr;
                    }
                    else
                    {
                        item = createNewSheetPin( sheet, cursorPos );

                        if( m_dialogSyncSheetPin && m_dialogSyncSheetPin->GetPlacementTemplate() )
                        {
                            auto label = static_cast<SCH_HIERLABEL*>(
                                    m_dialogSyncSheetPin->GetPlacementTemplate() );
                            auto pin = static_cast<SCH_HIERLABEL*>( item );
                            pin->SetText( label->GetText() );
                            pin->SetShape( label->GetShape() );
                        }
                    }

                    description = _( "Add Sheet Pin" );
                }

                // If we started with a hotkey which has a position then warp back to that.
                // Otherwise update to the current mouse position pinned inside the autoscroll
                // boundaries.
                if( evt->IsPrime() && !ignorePrimePosition )
                {
                    cursorPos = grid.Align( evt->Position() );
                    getViewControls()->WarpMouseCursor( cursorPos, true );
                }
                else
                {
                    getViewControls()->PinCursorInsideNonAutoscrollArea( true );
                    cursorPos = getViewControls()->GetMousePosition();
                    cursorPos = grid.BestSnapAnchor( cursorPos, snapGrid, item );
                }

                if( item )
                {
                    item->SetPosition( cursorPos );

                    item->SetFlags( IS_NEW | IS_MOVING );
                    item->AutoplaceFields( nullptr, false /* aManual */ );
                    updatePreview();
                    m_selectionTool->AddItemToSel( item );
                    m_toolMgr->PostAction( ACTIONS::refreshPreview );

                    // update the cursor so it looks correct before another event
                    setCursor();
                }

                controls->SetCursorPosition( cursorPos, false );
            }
            else            // ... and second click places:
            {
                SCH_COMMIT commit( m_toolMgr );

                item->ClearFlags( IS_MOVING );

                if( item->IsConnectable() )
                    m_frame->AutoRotateItem( m_frame->GetScreen(), item );

                if( isSheetPin )
                {
                    // Sheet pins are owned by their parent sheet.
                    commit.Modify( sheet, m_frame->GetScreen() );
                    sheet->AddPin( (SCH_SHEET_PIN*) item );
                }
                else
                {
                    m_frame->SaveCopyForRepeatItem( item );
                    m_frame->AddToScreen( item, m_frame->GetScreen() );
                    commit.Added( item, m_frame->GetScreen() );
                }

                item->AutoplaceFields( m_frame->GetScreen(), false /* aManual */ );

                commit.Push( description );

                m_view->ClearPreview();

                if( m_dialogSyncSheetPin && m_dialogSyncSheetPin->GetPlacementTemplate() )
                {
                    m_frame->PopTool( aEvent );
                    m_toolMgr->RunAction( EE_ACTIONS::clearSelection );
                    m_dialogSyncSheetPin->EndPlaceItem( item );
                    m_dialogSyncSheetPin->Show( true );
                    break;
                }
                else
                {
                    item = nullptr;
                }

                if( isSheetPin )
                {
                    item = createNewSheetPin( sheet, cursorPos );
                    item->SetPosition( cursorPos );
                    m_selectionTool->ClearSelection();
                    m_selectionTool->AddItemToSel( item );
                }
            }
        }
        else if( evt->IsClick( BUT_RIGHT ) )
        {
            // Warp after context menu only if dragging...
            if( !item )
                m_toolMgr->VetoContextMenuMouseWarp();

            m_menu.ShowContextMenu( m_selectionTool->GetSelection() );
        }
        else if( item && evt->IsSelectionEvent() )
        {
            // This happens if our text was replaced out from under us by ConvertTextType()
            EE_SELECTION& selection = m_selectionTool->GetSelection();

            if( selection.GetSize() == 1 )
            {
                item = (SCH_ITEM*) selection.Front();
                updatePreview();
            }
            else
            {
                item = nullptr;
            }
        }
        else if( evt->IsAction( &ACTIONS::duplicate )
                 || evt->IsAction( &EE_ACTIONS::repeatDrawItem ) )
        {
            if( item )
            {
                // This doesn't really make sense; we'll just end up dragging a stack of
                // objects so we ignore the duplicate and just carry on.
                wxBell();
                continue;
            }

            // Exit.  The duplicate will run in its own loop.
            m_frame->PopTool( aEvent );
            break;
        }
        else if( item && ( evt->IsAction( &ACTIONS::refreshPreview ) || evt->IsMotion() ) )
        {
            item->SetPosition( cursorPos );
            item->AutoplaceFields( /* aScreen */ nullptr, /* aManual */ false );
            updatePreview();
        }
        else if( item && evt->IsAction( &ACTIONS::doDelete ) )
        {
            cleanup();
        }
        else if( evt->IsAction( &ACTIONS::redo ) )
        {
            wxBell();
        }
        else
        {
            evt->SetPassEvent();
        }

        // Enable autopanning and cursor capture only when there is an item to be placed
        controls->SetAutoPan( item != nullptr );
        controls->CaptureCursor( item != nullptr );
    }

    controls->SetAutoPan( false );
    controls->CaptureCursor( false );
    controls->ForceCursorPosition( false );
    m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::ARROW );

    if( m_dialogSyncSheetPin && m_dialogSyncSheetPin->GetPlacementTemplate() )
    {
        m_dialogSyncSheetPin->EndPlaceItem( nullptr );
        m_dialogSyncSheetPin->Show( true );
    }

    return 0;
}


int SCH_DRAWING_TOOLS::DrawShape( const TOOL_EVENT& aEvent )
{
    SCHEMATIC*          schematic = getModel<SCHEMATIC>();
    SCHEMATIC_SETTINGS& sch_settings = schematic->Settings();
    SCH_SHAPE*          item = nullptr;
    bool                isTextBox = aEvent.IsAction( &EE_ACTIONS::drawTextBox );
    SHAPE_T             type = aEvent.Parameter<SHAPE_T>();
    wxString            description;

    if( m_inDrawingTool )
        return 0;

    REENTRANCY_GUARD guard( &m_inDrawingTool );

    KIGFX::VIEW_CONTROLS* controls = getViewControls();
    EE_GRID_HELPER        grid( m_toolMgr );
    VECTOR2I              cursorPos;

    // We might be running as the same shape in another co-routine.  Make sure that one
    // gets whacked.
    m_toolMgr->DeactivateTool();

    m_toolMgr->RunAction( EE_ACTIONS::clearSelection );

    m_frame->PushTool( aEvent );

    auto setCursor =
            [&]()
            {
                m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::PENCIL );
            };

    auto cleanup =
            [&] ()
            {
                m_toolMgr->RunAction( EE_ACTIONS::clearSelection );
                m_view->ClearPreview();
                delete item;
                item = nullptr;
            };

    Activate();

    // Must be done after Activate() so that it gets set into the correct context
    getViewControls()->ShowCursor( true );

    // Set initial cursor
    setCursor();

    if( aEvent.HasPosition() )
        m_toolMgr->PrimeTool( aEvent.Position() );

    // Main loop: keep receiving events
    while( TOOL_EVENT* evt = Wait() )
    {
        setCursor();
        grid.SetSnap( !evt->Modifier( MD_SHIFT ) );
        grid.SetUseGrid( getView()->GetGAL()->GetGridSnapping() && !evt->DisableGridSnapping() );

        cursorPos = grid.Align( controls->GetMousePosition(), GRID_HELPER_GRIDS::GRID_GRAPHICS );
        controls->ForceCursorPosition( true, cursorPos );

        // The tool hotkey is interpreted as a click when drawing
        bool isSyntheticClick = item && evt->IsActivate() && evt->HasPosition()
                                && evt->Matches( aEvent );

        if( evt->IsCancelInteractive() || ( item && evt->IsAction( &ACTIONS::undo ) ) )
        {
            if( item )
            {
                cleanup();
            }
            else
            {
                m_frame->PopTool( aEvent );
                break;
            }
        }
        else if( evt->IsActivate() && !isSyntheticClick )
        {
            if( item && evt->IsMoveTool() )
            {
                // we're already drawing our own item; ignore the move tool
                evt->SetPassEvent( false );
                continue;
            }

            if( item )
                cleanup();

            if( evt->IsPointEditor() )
            {
                // don't exit (the point editor runs in the background)
            }
            else if( evt->IsMoveTool() )
            {
                // leave ourselves on the stack so we come back after the move
                break;
            }
            else
            {
                m_frame->PopTool( aEvent );
                break;
            }
        }
        else if( evt->IsClick( BUT_LEFT ) && !item )
        {
            m_toolMgr->RunAction( EE_ACTIONS::clearSelection );

            if( isTextBox )
            {
                SCH_TEXTBOX* textbox = new SCH_TEXTBOX( LAYER_NOTES, 0, m_lastTextboxFillStyle );

                textbox->SetTextSize( VECTOR2I( sch_settings.m_DefaultTextSize,
                                                sch_settings.m_DefaultTextSize ) );

                // Must come after SetTextSize()
                textbox->SetBold( m_lastTextBold );
                textbox->SetItalic( m_lastTextItalic );

                textbox->SetTextAngle( m_lastTextboxAngle );
                textbox->SetHorizJustify( m_lastTextboxHJustify );
                textbox->SetVertJustify( m_lastTextboxVJustify );
                textbox->SetStroke( m_lastTextboxStroke );
                textbox->SetFillColor( m_lastTextboxFillColor );
                textbox->SetParent( schematic );

                item = textbox;
                description = _( "Add Text Box" );
            }
            else
            {
                item = new SCH_SHAPE( type, LAYER_NOTES, 0, m_lastFillStyle );

                item->SetStroke( m_lastStroke );
                item->SetFillColor( m_lastFillColor );
                item->SetParent( schematic );
                description = wxString::Format( _( "Add %s" ), item->GetFriendlyName() );
            }

            item->SetFlags( IS_NEW );
            item->BeginEdit( cursorPos );

            m_view->ClearPreview();
            m_view->AddToPreview( item->Clone() );
        }
        else if( item && ( evt->IsClick( BUT_LEFT )
                        || evt->IsDblClick( BUT_LEFT )
                        || isSyntheticClick
                        || evt->IsAction( &ACTIONS::finishInteractive ) ) )
        {
            if( evt->IsDblClick( BUT_LEFT )
                    || evt->IsAction( &ACTIONS::finishInteractive )
                    || !item->ContinueEdit( cursorPos ) )
            {
                item->EndEdit();
                item->ClearEditFlags();
                item->SetFlags( IS_NEW );

                if( isTextBox )
                {
                    SCH_TEXTBOX*           textbox = static_cast<SCH_TEXTBOX*>( item );
                    DIALOG_TEXT_PROPERTIES dlg( m_frame, textbox );

                    getViewControls()->SetAutoPan( false );
                    getViewControls()->CaptureCursor( false );

                    // QuasiModal required for syntax help and Scintilla auto-complete
                    if( dlg.ShowQuasiModal() != wxID_OK )
                    {
                        cleanup();
                        continue;
                    }

                    m_lastTextBold = textbox->IsBold();
                    m_lastTextItalic = textbox->IsItalic();
                    m_lastTextboxAngle = textbox->GetTextAngle();
                    m_lastTextboxHJustify = textbox->GetHorizJustify();
                    m_lastTextboxVJustify = textbox->GetVertJustify();
                    m_lastTextboxStroke = textbox->GetStroke();
                    m_lastTextboxFillStyle = textbox->GetFillMode();
                    m_lastTextboxFillColor = textbox->GetFillColor();
                }
                else
                {
                    m_lastStroke = item->GetStroke();
                    m_lastFillStyle = item->GetFillMode();
                    m_lastFillColor = item->GetFillColor();
                }

                SCH_COMMIT commit( m_toolMgr );
                commit.Add( item, m_frame->GetScreen() );
                commit.Push( wxString::Format( _( "Draw %s" ), item->GetClass() ) );

                m_selectionTool->AddItemToSel( item );
                item = nullptr;

                m_view->ClearPreview();
                m_toolMgr->PostAction( ACTIONS::activatePointEditor );
            }
        }
        else if( evt->IsAction( &ACTIONS::duplicate )
                 || evt->IsAction( &EE_ACTIONS::repeatDrawItem ) )
        {
            if( item )
            {
                // This doesn't really make sense; we'll just end up dragging a stack of
                // objects so we ignore the duplicate and just carry on.
                wxBell();
                continue;
            }

            // Exit.  The duplicate will run in its own loop.
            m_frame->PopTool( aEvent );
            break;
        }
        else if( item && ( evt->IsAction( &ACTIONS::refreshPreview ) || evt->IsMotion() ) )
        {
            item->CalcEdit( cursorPos );
            m_view->ClearPreview();
            m_view->AddToPreview( item->Clone() );
            m_frame->SetMsgPanel( item );
        }
        else if( evt->IsDblClick( BUT_LEFT ) && !item )
        {
            m_toolMgr->RunAction( EE_ACTIONS::properties );
        }
        else if( evt->IsClick( BUT_RIGHT ) )
        {
            // Warp after context menu only if dragging...
            if( !item )
                m_toolMgr->VetoContextMenuMouseWarp();

            m_menu.ShowContextMenu( m_selectionTool->GetSelection() );
        }
        else if( item && evt->IsAction( &ACTIONS::redo ) )
        {
            wxBell();
        }
        else
        {
            evt->SetPassEvent();
        }

        // Enable autopanning and cursor capture only when there is a shape being drawn
        getViewControls()->SetAutoPan( item != nullptr );
        getViewControls()->CaptureCursor( item != nullptr );
    }

    getViewControls()->SetAutoPan( false );
    getViewControls()->CaptureCursor( false );
    m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::ARROW );
    return 0;
}


int SCH_DRAWING_TOOLS::DrawRuleArea( const TOOL_EVENT& aEvent )
{
    if( m_inDrawingTool )
        return 0;

    REENTRANCY_GUARD       guard( &m_inDrawingTool );
    SCOPED_SET_RESET<bool> scopedDrawMode( m_drawingRuleArea, true );

    KIGFX::VIEW_CONTROLS* controls = getViewControls();
    EE_GRID_HELPER        grid( m_toolMgr );
    VECTOR2I              cursorPos;

    RULE_AREA_CREATE_HELPER ruleAreaTool( *getView(), m_frame, m_toolMgr );
    POLYGON_GEOM_MANAGER    polyGeomMgr( ruleAreaTool );
    bool                    started = false;

    // We might be running as the same shape in another co-routine.  Make sure that one
    // gets whacked.
    m_toolMgr->DeactivateTool();

    m_toolMgr->RunAction( EE_ACTIONS::clearSelection );

    m_frame->PushTool( aEvent );

    auto setCursor =
            [&]()
            {
                m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::PENCIL );
            };

    auto cleanup = [&]()
    {
        polyGeomMgr.Reset();
        started = false;
        getViewControls()->SetAutoPan( false );
        getViewControls()->CaptureCursor( false );
        m_toolMgr->RunAction( EE_ACTIONS::clearSelection );
    };

    Activate();

    // Must be done after Activate() so that it gets set into the correct context
    getViewControls()->ShowCursor( true );
    //m_controls->ForceCursorPosition( false );

    // Set initial cursor
    setCursor();

    if( aEvent.HasPosition() )
        m_toolMgr->PrimeTool( aEvent.Position() );

    // Main loop: keep receiving events
    while( TOOL_EVENT* evt = Wait() )
    {
        setCursor();

        grid.SetSnap( !evt->Modifier( MD_SHIFT ) );
        grid.SetUseGrid( getView()->GetGAL()->GetGridSnapping() && !evt->DisableGridSnapping() );

        cursorPos = grid.Align( controls->GetMousePosition(), GRID_HELPER_GRIDS::GRID_CONNECTABLE );
        controls->ForceCursorPosition( true, cursorPos );

        polyGeomMgr.SetLeaderMode( m_frame->eeconfig()->m_Drawing.line_mode == LINE_MODE_FREE
                                           ? POLYGON_GEOM_MANAGER::LEADER_MODE::DIRECT
                                           : POLYGON_GEOM_MANAGER::LEADER_MODE::DEG45 );

        if( evt->IsCancelInteractive() )
        {
            if( started )
            {
                cleanup();
            }
            else
            {
                m_frame->PopTool( aEvent );

                // We've handled the cancel event.  Don't cancel other tools
                evt->SetPassEvent( false );
                break;
            }
        }
        else if( evt->IsActivate() )
        {
            if( started )
                cleanup();

            if( evt->IsPointEditor() )
            {
                // don't exit (the point editor runs in the background)
            }
            else if( evt->IsMoveTool() )
            {
                // leave ourselves on the stack so we come back after the move
                break;
            }
            else
            {
                m_frame->PopTool( aEvent );
                break;
            }
        }
        else if( evt->IsClick( BUT_RIGHT ) )
        {
            if( !started )
                m_toolMgr->VetoContextMenuMouseWarp();

            m_menu.ShowContextMenu( m_selectionTool->GetSelection() );
        }
        // events that lock in nodes
        else if( evt->IsClick( BUT_LEFT ) || evt->IsDblClick( BUT_LEFT )
                 || evt->IsAction( &EE_ACTIONS::closeOutline ) )
        {
            // Check if it is double click / closing line (so we have to finish the zone)
            const bool endPolygon = evt->IsDblClick( BUT_LEFT )
                                    || evt->IsAction( &EE_ACTIONS::closeOutline )
                                    || polyGeomMgr.NewPointClosesOutline( cursorPos );

            if( endPolygon )
            {
                polyGeomMgr.SetFinished();
                polyGeomMgr.Reset();

                started = false;
                getViewControls()->SetAutoPan( false );
                getViewControls()->CaptureCursor( false );
            }
            // adding a corner
            else if( polyGeomMgr.AddPoint( cursorPos ) )
            {
                if( !started )
                {
                    started = true;

                    getViewControls()->SetAutoPan( true );
                    getViewControls()->CaptureCursor( true );
                }
            }
        }
        else if( started
                 && ( evt->IsAction( &EE_ACTIONS::deleteLastPoint )
                      || evt->IsAction( &ACTIONS::doDelete ) || evt->IsAction( &ACTIONS::undo ) ) )
        {
            if( std::optional<VECTOR2I> last = polyGeomMgr.DeleteLastCorner() )
            {
                cursorPos = last.value();
                getViewControls()->WarpMouseCursor( cursorPos, true );
                getViewControls()->ForceCursorPosition( true, cursorPos );
                polyGeomMgr.SetCursorPosition( cursorPos );
            }
            else
            {
                cleanup();
            }
        }
        else if( started && ( evt->IsMotion() || evt->IsDrag( BUT_LEFT ) ) )
        {
            polyGeomMgr.SetCursorPosition( cursorPos );
        }
        else
        {
            evt->SetPassEvent();
        }

    } // end while

    getViewControls()->SetAutoPan( false );
    getViewControls()->CaptureCursor( false );
    m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::ARROW );
    return 0;
}


int SCH_DRAWING_TOOLS::DrawTable( const TOOL_EVENT& aEvent )
{
    SCHEMATIC* schematic = getModel<SCHEMATIC>();
    SCH_TABLE* table = nullptr;

    if( m_inDrawingTool )
        return 0;

    REENTRANCY_GUARD guard( &m_inDrawingTool );

    KIGFX::VIEW_CONTROLS* controls = getViewControls();
    EE_GRID_HELPER        grid( m_toolMgr );
    VECTOR2I              cursorPos;

    // We might be running as the same shape in another co-routine.  Make sure that one
    // gets whacked.
    m_toolMgr->DeactivateTool();

    m_toolMgr->RunAction( EE_ACTIONS::clearSelection );

    m_frame->PushTool( aEvent );

    auto setCursor =
            [&]()
            {
                m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::PENCIL );
            };

    auto cleanup =
            [&] ()
            {
                m_toolMgr->RunAction( EE_ACTIONS::clearSelection );
                m_view->ClearPreview();
                delete table;
                table = nullptr;
            };

    Activate();

    // Must be done after Activate() so that it gets set into the correct context
    getViewControls()->ShowCursor( true );

    // Set initial cursor
    setCursor();

    if( aEvent.HasPosition() )
        m_toolMgr->PrimeTool( aEvent.Position() );

    // Main loop: keep receiving events
    while( TOOL_EVENT* evt = Wait() )
    {
        setCursor();
        grid.SetSnap( !evt->Modifier( MD_SHIFT ) );
        grid.SetUseGrid( getView()->GetGAL()->GetGridSnapping() && !evt->DisableGridSnapping() );

        cursorPos = grid.Align( controls->GetMousePosition(), GRID_HELPER_GRIDS::GRID_GRAPHICS );
        controls->ForceCursorPosition( true, cursorPos );

        // The tool hotkey is interpreted as a click when drawing
        bool isSyntheticClick = table && evt->IsActivate() && evt->HasPosition()
                                && evt->Matches( aEvent );

        if( evt->IsCancelInteractive() || ( table && evt->IsAction( &ACTIONS::undo ) ) )
        {
            if( table )
            {
                cleanup();
            }
            else
            {
                m_frame->PopTool( aEvent );
                break;
            }
        }
        else if( evt->IsActivate() && !isSyntheticClick )
        {
            if( table && evt->IsMoveTool() )
            {
                // we're already drawing our own item; ignore the move tool
                evt->SetPassEvent( false );
                continue;
            }

            if( table )
                cleanup();

            if( evt->IsPointEditor() )
            {
                // don't exit (the point editor runs in the background)
            }
            else if( evt->IsMoveTool() )
            {
                // leave ourselves on the stack so we come back after the move
                break;
            }
            else
            {
                m_frame->PopTool( aEvent );
                break;
            }
        }
        else if( evt->IsClick( BUT_LEFT ) && !table )
        {
            m_toolMgr->RunAction( EE_ACTIONS::clearSelection );

            table = new SCH_TABLE( 0 );
            table->SetColCount( 1 );

            SCH_TABLECELL* tableCell = new SCH_TABLECELL();
            int defaultTextSize = schematic->Settings().m_DefaultTextSize;

            tableCell->SetTextSize( VECTOR2I( defaultTextSize, defaultTextSize ) );
            table->AddCell( tableCell );

            table->SetParent( schematic );
            table->SetFlags( IS_NEW );
            table->SetPosition( cursorPos );

            m_view->ClearPreview();
            m_view->AddToPreview( table->Clone() );
        }
        else if( table && (   evt->IsClick( BUT_LEFT )
                           || evt->IsDblClick( BUT_LEFT )
                           || isSyntheticClick
                           || evt->IsAction( &EE_ACTIONS::finishInteractive ) ) )
        {
            table->ClearEditFlags();
            table->SetFlags( IS_NEW );
            table->Normalize();

            DIALOG_TABLE_PROPERTIES dlg( m_frame, table );

            // QuasiModal required for Scintilla auto-complete
            if( dlg.ShowQuasiModal() == wxID_OK )
            {
                SCH_COMMIT commit( m_toolMgr );
                commit.Add( table, m_frame->GetScreen() );
                commit.Push( _( "Draw Table" ) );

                m_selectionTool->AddItemToSel( table );
                m_toolMgr->PostAction( ACTIONS::activatePointEditor );
            }
            else
            {
                delete table;
            }

            table = nullptr;
            m_view->ClearPreview();
        }
        else if( table && ( evt->IsAction( &ACTIONS::refreshPreview ) || evt->IsMotion() ) )
        {
            VECTOR2I gridSize = grid.GetGridSize( grid.GetItemGrid( table ) );
            int      fontSize = schematic->Settings().m_DefaultTextSize;
            VECTOR2I origin( table->GetPosition() );
            VECTOR2I requestedSize( cursorPos - origin );

            int colCount = std::max( 1, requestedSize.x / ( fontSize * 15 ) );
            int rowCount = std::max( 1, requestedSize.y / ( fontSize * 2  ) );

            VECTOR2I cellSize( std::max( gridSize.x * 5, requestedSize.x / colCount ),
                               std::max( gridSize.y * 2, requestedSize.y / rowCount ) );

            cellSize.x = KiROUND( (double) cellSize.x / gridSize.x ) * gridSize.x;
            cellSize.y = KiROUND( (double) cellSize.y / gridSize.y ) * gridSize.y;

            table->ClearCells();
            table->SetColCount( colCount );

            for( int col = 0; col < colCount; ++col )
                table->SetColWidth( col, cellSize.x );

            for( int row = 0; row < rowCount; ++row )
            {
                table->SetRowHeight( row, cellSize.y );

                for( int col = 0; col < colCount; ++col )
                {
                    SCH_TABLECELL* cell = new SCH_TABLECELL();
                    int defaultTextSize = schematic->Settings().m_DefaultTextSize;

                    cell->SetTextSize( VECTOR2I( defaultTextSize, defaultTextSize ) );
                    cell->SetPosition( origin + VECTOR2I( col * cellSize.x, row * cellSize.y ) );
                    cell->SetEnd( cell->GetPosition() + cellSize );
                    table->AddCell( cell );
                }
            }

            m_view->ClearPreview();
            m_view->AddToPreview( table->Clone() );
            m_frame->SetMsgPanel( table );
        }
        else if( evt->IsDblClick( BUT_LEFT ) && !table )
        {
            m_toolMgr->RunAction( EE_ACTIONS::properties );
        }
        else if( evt->IsClick( BUT_RIGHT ) )
        {
            // Warp after context menu only if dragging...
            if( !table )
                m_toolMgr->VetoContextMenuMouseWarp();

            m_menu.ShowContextMenu( m_selectionTool->GetSelection() );
        }
        else if( table && evt->IsAction( &ACTIONS::redo ) )
        {
            wxBell();
        }
        else
        {
            evt->SetPassEvent();
        }

        // Enable autopanning and cursor capture only when there is a shape being drawn
        getViewControls()->SetAutoPan( table != nullptr );
        getViewControls()->CaptureCursor( table != nullptr );
    }

    getViewControls()->SetAutoPan( false );
    getViewControls()->CaptureCursor( false );
    m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::ARROW );
    return 0;
}


int SCH_DRAWING_TOOLS::DrawSheet( const TOOL_EVENT& aEvent )
{
    SCH_SHEET* sheet = nullptr;

    if( m_inDrawingTool )
        return 0;

    REENTRANCY_GUARD guard( &m_inDrawingTool );

    KIGFX::VIEW_CONTROLS* controls = getViewControls();
    EE_GRID_HELPER        grid( m_toolMgr );
    VECTOR2I              cursorPos;

    m_toolMgr->RunAction( EE_ACTIONS::clearSelection );

    m_frame->PushTool( aEvent );

    auto setCursor =
            [&]()
            {
                m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::PENCIL );
            };

    auto cleanup =
            [&] ()
            {
                m_toolMgr->RunAction( EE_ACTIONS::clearSelection );
                m_view->ClearPreview();
                delete sheet;
                sheet = nullptr;
            };

    Activate();

    // Must be done after Activate() so that it gets set into the correct context
    getViewControls()->ShowCursor( true );

    // Set initial cursor
    setCursor();

    if( aEvent.HasPosition() )
        m_toolMgr->PrimeTool( aEvent.Position() );

    // Main loop: keep receiving events
    while( TOOL_EVENT* evt = Wait() )
    {
        setCursor();
        grid.SetSnap( !evt->Modifier( MD_SHIFT ) );
        grid.SetUseGrid( getView()->GetGAL()->GetGridSnapping() && !evt->DisableGridSnapping() );

        cursorPos = grid.Align( controls->GetMousePosition(), GRID_HELPER_GRIDS::GRID_GRAPHICS );
        controls->ForceCursorPosition( true, cursorPos );

        // The tool hotkey is interpreted as a click when drawing
        bool isSyntheticClick = sheet && evt->IsActivate() && evt->HasPosition()
                                && evt->Matches( aEvent );

        if( evt->IsCancelInteractive() || ( sheet && evt->IsAction( &ACTIONS::undo ) ) )
        {
            m_frame->GetInfoBar()->Dismiss();

            if( sheet )
            {
                cleanup();
            }
            else
            {
                m_frame->PopTool( aEvent );
                break;
            }
        }
        else if( evt->IsActivate() && !isSyntheticClick )
        {
            if( sheet && evt->IsMoveTool() )
            {
                // we're already drawing our own item; ignore the move tool
                evt->SetPassEvent( false );
                continue;
            }

            if( sheet )
            {
                m_frame->ShowInfoBarMsg( _( "Press <ESC> to cancel sheet creation." ) );
                evt->SetPassEvent( false );
                continue;
            }

            if( evt->IsPointEditor() )
            {
                // don't exit (the point editor runs in the background)
            }
            else if( evt->IsMoveTool() )
            {
                // leave ourselves on the stack so we come back after the move
                break;
            }
            else
            {
                m_frame->PopTool( aEvent );
                break;
            }
        }
        else if( !sheet && ( evt->IsClick( BUT_LEFT ) || evt->IsDblClick( BUT_LEFT ) ) )
        {
            EE_SELECTION&      selection = m_selectionTool->GetSelection();
            EESCHEMA_SETTINGS* cfg = m_frame->eeconfig();

            if( selection.Size() == 1
                    && selection.Front()->Type() == SCH_SHEET_T
                    && selection.Front()->GetBoundingBox().Contains( cursorPos ) )
            {
                if( evt->IsClick( BUT_LEFT ) )
                {
                    // sheet already selected
                    continue;
                }
                else if( evt->IsDblClick( BUT_LEFT ) )
                {
                    m_toolMgr->PostAction( EE_ACTIONS::enterSheet );
                    break;
                }
            }

            m_toolMgr->RunAction( EE_ACTIONS::clearSelection );

            sheet = new SCH_SHEET( m_frame->GetCurrentSheet().Last(), cursorPos );
            sheet->SetFlags( IS_NEW | IS_MOVING );
            sheet->SetScreen( nullptr );
            sheet->SetBorderWidth( schIUScale.MilsToIU( cfg->m_Drawing.default_line_thickness ) );
            sheet->SetBorderColor( cfg->m_Drawing.default_sheet_border_color );
            sheet->SetBackgroundColor( cfg->m_Drawing.default_sheet_background_color );
            sheet->GetFields()[ SHEETNAME ].SetText( "Untitled Sheet" );
            sheet->GetFields()[ SHEETFILENAME ].SetText( "untitled." + FILEEXT::KiCadSchematicFileExtension );
            sizeSheet( sheet, cursorPos );

            m_view->ClearPreview();
            m_view->AddToPreview( sheet->Clone() );
        }
        else if( sheet && ( evt->IsClick( BUT_LEFT )
                         || evt->IsDblClick( BUT_LEFT )
                         || isSyntheticClick
                         || evt->IsAction( &ACTIONS::finishInteractive ) ) )
        {
            getViewControls()->SetAutoPan( false );
            getViewControls()->CaptureCursor( false );

            if( m_frame->EditSheetProperties( static_cast<SCH_SHEET*>( sheet ),
                                              &m_frame->GetCurrentSheet() ) )
            {
                m_view->ClearPreview();

                sheet->AutoplaceFields( /* aScreen */ nullptr, /* aManual */ false );

                SCH_COMMIT commit( m_toolMgr );
                commit.Add( sheet, m_frame->GetScreen() );
                commit.Push( "Draw Sheet" );

                SCH_SHEET_PATH newPath = m_frame->GetCurrentSheet();
                newPath.push_back( sheet );

                m_frame->UpdateHierarchyNavigator();
                m_selectionTool->AddItemToSel( sheet );
            }
            else
            {
                m_view->ClearPreview();
                delete sheet;
            }

            sheet = nullptr;
        }
        else if( evt->IsAction( &ACTIONS::duplicate )
                 || evt->IsAction( &EE_ACTIONS::repeatDrawItem ) )
        {
            if( sheet )
            {
                // This doesn't really make sense; we'll just end up dragging a stack of
                // objects so we ignore the duplicate and just carry on.
                wxBell();
                continue;
            }

            // Exit.  The duplicate will run in its own loop.
            m_frame->PopTool( aEvent );
            break;
        }
        else if( sheet && ( evt->IsAction( &ACTIONS::refreshPreview ) || evt->IsMotion() ) )
        {
            sizeSheet( sheet, cursorPos );
            m_view->ClearPreview();
            m_view->AddToPreview( sheet->Clone() );
            m_frame->SetMsgPanel( sheet );
        }
        else if( evt->IsClick( BUT_RIGHT ) )
        {
            // Warp after context menu only if dragging...
            if( !sheet )
                m_toolMgr->VetoContextMenuMouseWarp();

            m_menu.ShowContextMenu( m_selectionTool->GetSelection() );
        }
        else if( sheet && evt->IsAction( &ACTIONS::redo ) )
        {
            wxBell();
        }
        else
        {
            evt->SetPassEvent();
        }

        // Enable autopanning and cursor capture only when there is a sheet to be placed
        getViewControls()->SetAutoPan( sheet != nullptr );
        getViewControls()->CaptureCursor( sheet != nullptr );
    }

    getViewControls()->SetAutoPan( false );
    getViewControls()->CaptureCursor( false );
    m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::ARROW );

    return 0;
}


void SCH_DRAWING_TOOLS::sizeSheet( SCH_SHEET* aSheet, const VECTOR2I& aPos )
{
    VECTOR2I pos = aSheet->GetPosition();
    VECTOR2I size = aPos - pos;

    size.x = std::max( size.x, schIUScale.MilsToIU( MIN_SHEET_WIDTH ) );
    size.y = std::max( size.y, schIUScale.MilsToIU( MIN_SHEET_HEIGHT ) );

    VECTOR2I grid = m_frame->GetNearestGridPosition( pos + size );
    aSheet->Resize( VECTOR2I( grid.x - pos.x, grid.y - pos.y ) );
}


int SCH_DRAWING_TOOLS::doSyncSheetsPins( std::list<SCH_SHEET_PATH> sheetPaths )
{
    if( !sheetPaths.size() )
        return 0;

    m_dialogSyncSheetPin = std::make_unique<DIALOG_SYNC_SHEET_PINS>(
            m_frame, std::move( sheetPaths ),
            std::make_shared<SHEET_SYNCHRONIZATION_AGENT>(
                    [&]( EDA_ITEM* aItem, SCH_SHEET_PATH aPath,
                         SHEET_SYNCHRONIZATION_AGENT::MODIFICATION const& aModify )
                    {
                        SCH_COMMIT commit( m_toolMgr );

                        if( auto pin = dynamic_cast<SCH_SHEET_PIN*>( aItem ) )
                        {
                            commit.Modify( pin->GetParent(), aPath.LastScreen() );
                            aModify();
                            commit.Push( _( "Modify sheet pin" ) );
                        }
                        else
                        {
                            commit.Modify( aItem, aPath.LastScreen() );
                            aModify();
                            commit.Push( _( "Modify schematic item" ) );
                        }

                        updateItem( aItem, true );
                        m_frame->OnModify();
                    },
                    [&]( EDA_ITEM* aItem, SCH_SHEET_PATH aPath )
                    {
                        m_frame->GetToolManager()->RunAction<SCH_SHEET_PATH*>(
                                EE_ACTIONS::changeSheet, &aPath );
                        EE_SELECTION_TOOL* selectionTool = m_toolMgr->GetTool<EE_SELECTION_TOOL>();
                        selectionTool->UnbrightenItem( aItem );
                        selectionTool->AddItemToSel( aItem, true );
                        m_toolMgr->RunAction( ACTIONS::doDelete );
                    },
                    [&]( SCH_SHEET* aItem, SCH_SHEET_PATH aPath,
                         SHEET_SYNCHRONIZATION_AGENT::SHEET_SYNCHRONIZATION_PLACEMENT aOp,
                         EDA_ITEM*                                                    aTemplate )
                    {
                        switch( aOp )
                        {
                        case SHEET_SYNCHRONIZATION_AGENT::PLACE_HIERLABEL:
                        {
                            SCH_SHEET* sheet = static_cast<SCH_SHEET*>( aItem );
                            m_dialogSyncSheetPin->Hide();
                            m_dialogSyncSheetPin->BeginPlaceItem(
                                    sheet, DIALOG_SYNC_SHEET_PINS::PlaceItemKind::HIERLABEL,
                                    aTemplate );
                            m_frame->GetToolManager()->RunAction<SCH_SHEET_PATH*>(
                                    EE_ACTIONS::changeSheet, &aPath );
                            m_toolMgr->RunAction( EE_ACTIONS::placeHierLabel );
                            break;
                        }
                        case SHEET_SYNCHRONIZATION_AGENT::PLACE_SHEET_PIN:
                        {
                            SCH_SHEET* sheet = static_cast<SCH_SHEET*>( aItem );
                            m_dialogSyncSheetPin->Hide();
                            m_dialogSyncSheetPin->BeginPlaceItem(
                                    sheet, DIALOG_SYNC_SHEET_PINS::PlaceItemKind::SHEET_PIN,
                                    aTemplate );
                            m_frame->GetToolManager()->RunAction<SCH_SHEET_PATH*>(
                                    EE_ACTIONS::changeSheet, &aPath );
                            m_toolMgr->GetTool<EE_SELECTION_TOOL>()->SyncSelection( {}, nullptr,
                                                                                    { sheet } );
                            m_toolMgr->RunAction( EE_ACTIONS::placeSheetPin );
                            break;
                        }
                        }
                    },
                    m_toolMgr, m_frame ) );
    m_dialogSyncSheetPin->Show( true );
    return 0;
}


int SCH_DRAWING_TOOLS::SyncSheetsPins( const TOOL_EVENT& aEvent )
{
    SCH_SHEET* sheet = dynamic_cast<SCH_SHEET*>( m_selectionTool->GetSelection().Front() );

    if( !sheet )
    {
        VECTOR2I cursorPos =  getViewControls()->GetMousePosition();

        if( EDA_ITEM* i = nullptr; static_cast<void>(m_selectionTool->SelectPoint( cursorPos, { SCH_SHEET_T }, &i ) ) , i != nullptr )
        {
            sheet = dynamic_cast<SCH_SHEET*>( i );
        }
    }

    if ( sheet )
    {
        SCH_SHEET_PATH current = m_frame->GetCurrentSheet();
        current.push_back( sheet );
        return doSyncSheetsPins( { current } );
    }

    return 0;
}


int SCH_DRAWING_TOOLS::SyncAllSheetsPins( const TOOL_EVENT& aEvent )
{
    static const std::function<void( std::list<SCH_SHEET_PATH>&, SCH_SCREEN*,
                                     std::set<SCH_SCREEN*>&, SCH_SHEET_PATH const& )>
            getSheetChildren = []( std::list<SCH_SHEET_PATH>& aPaths, SCH_SCREEN* aScene,
                                   std::set<SCH_SCREEN*>& aVisited, SCH_SHEET_PATH const& aCurPath )
    {
        if( ! aScene || aVisited.find(aScene) != aVisited.end() )
            return ;

        std::vector<SCH_ITEM*> sheetChildren;
        aScene->GetSheets( &sheetChildren );
        aVisited.insert( aScene );

        for( SCH_ITEM* child : sheetChildren )
        {
            SCH_SHEET_PATH cp = aCurPath;
            SCH_SHEET* sheet = static_cast<SCH_SHEET*>( child );
            cp.push_back( sheet );
            aPaths.push_back( cp );
            getSheetChildren( aPaths, sheet->GetScreen(), aVisited, cp );
        }
    };

    std::list<SCH_SHEET_PATH> sheetPaths;
    std::set<SCH_SCREEN*> visited;
    SCH_SHEET_PATH current;
    current.push_back( &m_frame->Schematic().Root() );
    getSheetChildren( sheetPaths, m_frame->Schematic().Root().GetScreen(), visited, current );
    return doSyncSheetsPins( std::move( sheetPaths ) );
}


void SCH_DRAWING_TOOLS::setTransitions()
{
    Go( &SCH_DRAWING_TOOLS::PlaceSymbol,         EE_ACTIONS::placeSymbol.MakeEvent() );
    Go( &SCH_DRAWING_TOOLS::PlaceSymbol,         EE_ACTIONS::placePower.MakeEvent() );
    Go( &SCH_DRAWING_TOOLS::SingleClickPlace,    EE_ACTIONS::placeNoConnect.MakeEvent() );
    Go( &SCH_DRAWING_TOOLS::SingleClickPlace,    EE_ACTIONS::placeJunction.MakeEvent() );
    Go( &SCH_DRAWING_TOOLS::SingleClickPlace,    EE_ACTIONS::placeBusWireEntry.MakeEvent() );
    Go( &SCH_DRAWING_TOOLS::TwoClickPlace,       EE_ACTIONS::placeLabel.MakeEvent() );
    Go( &SCH_DRAWING_TOOLS::TwoClickPlace,       EE_ACTIONS::placeClassLabel.MakeEvent() );
    Go( &SCH_DRAWING_TOOLS::TwoClickPlace,       EE_ACTIONS::placeHierLabel.MakeEvent() );
    Go( &SCH_DRAWING_TOOLS::TwoClickPlace,       EE_ACTIONS::placeGlobalLabel.MakeEvent() );
    Go( &SCH_DRAWING_TOOLS::DrawSheet,           EE_ACTIONS::drawSheet.MakeEvent() );
    Go( &SCH_DRAWING_TOOLS::TwoClickPlace,       EE_ACTIONS::placeSheetPin.MakeEvent() );
    Go( &SCH_DRAWING_TOOLS::TwoClickPlace,       EE_ACTIONS::placeSchematicText.MakeEvent() );
    Go( &SCH_DRAWING_TOOLS::DrawShape,           EE_ACTIONS::drawRectangle.MakeEvent() );
    Go( &SCH_DRAWING_TOOLS::DrawShape,           EE_ACTIONS::drawCircle.MakeEvent() );
    Go( &SCH_DRAWING_TOOLS::DrawShape,           EE_ACTIONS::drawArc.MakeEvent() );
    Go( &SCH_DRAWING_TOOLS::DrawShape,           EE_ACTIONS::drawTextBox.MakeEvent() );
    Go( &SCH_DRAWING_TOOLS::DrawRuleArea,        EE_ACTIONS::drawRuleArea.MakeEvent() );
    Go( &SCH_DRAWING_TOOLS::DrawTable,           EE_ACTIONS::drawTable.MakeEvent() );
    Go( &SCH_DRAWING_TOOLS::PlaceImage,          EE_ACTIONS::placeImage.MakeEvent() );
    Go( &SCH_DRAWING_TOOLS::ImportGraphics,      EE_ACTIONS::importGraphics.MakeEvent() );
    Go( &SCH_DRAWING_TOOLS::SyncSheetsPins,      EE_ACTIONS::syncSheetPins.MakeEvent() );
    Go( &SCH_DRAWING_TOOLS::SyncAllSheetsPins,   EE_ACTIONS::syncAllSheetsPins.MakeEvent() );
}
