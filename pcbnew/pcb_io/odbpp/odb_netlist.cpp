/**
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2023 KiCad Developers, see AUTHORS.txt for contributors.
 * Author: SYSUEric <jzzhuang666@gmail.com>.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <confirm.h>
#include <gestfich.h>
#include <kiface_base.h>
#include <pcb_edit_frame.h>
#include <trigo.h>
#include <build_version.h>
#include <macros.h>
#include <wildcards_and_files_ext.h>
#include <locale_io.h>
#include <board.h>
#include <board_design_settings.h>
#include <footprint.h>
#include <pad.h>
#include <pcb_track.h>
#include <vector>
#include <cctype>
#include <math/util.h>      // for KiROUND
#include <odb_netlist.h>
#include <wx/filedlg.h>
#include "odb_util.h"

// Compute the side code for a pad. Returns "" if there is no copper
std::string ODB_NET_LIST::ComputePadAccessSide( BOARD *aBoard, LSET aLayerMask )
{
    // Non-copper is not interesting here
    aLayerMask &= LSET::AllCuMask();
    if( !aLayerMask.any() )
        return "";

    // Traditional TH pad
    if( aLayerMask[F_Cu] && aLayerMask[B_Cu] )
        return "B";

    // Front SMD pad
    if( aLayerMask[F_Cu] )
        return "T";

    // Back SMD pad
    if( aLayerMask[B_Cu] )
        return "D";

    // Inner
    for( int layer = In1_Cu; layer < B_Cu; ++layer )
    {
        if( aLayerMask[layer] )
            return "I";
    }

    // This shouldn't happen
    return "";
}

// /* Convert and clamp a size from IU to decimils */
// int ODB_NET_LIST::iu_to_d356(int iu, int clamp)
// {
//     int val = KiROUND( iu / ( pcbIUScale.IU_PER_MILS / 10 ) );
//     if( val > clamp ) return clamp;
//     if( val < -clamp ) return -clamp;
//     return val;
// }


void ODB_NET_LIST::InitPadNetPoints( BOARD *aBoard, std::map<size_t, std::vector<ODB_NET_RECORD>>& aRecords )
{
    VECTOR2I origin = aBoard->GetDesignSettings().GetAuxOrigin();

    for( FOOTPRINT* footprint : aBoard->Footprints() )
    {
        for( PAD* pad : footprint->Pads() )
        {
            ODB_NET_RECORD net_point;
            net_point.side = ComputePadAccessSide( aBoard, pad->GetLayerSet() );

            // It could be a mask only pad, we only handle pads with copper here
            if( !net_point.side.empty() && net_point.side != "I" )
            {
                net_point.netname = pad->GetNetname();
                // net_point.pin = pad->GetNumber();
                net_point.refdes = footprint->GetReference();
                const VECTOR2I& drill = pad->GetDrillSize();
                net_point.hole = pad->HasHole();
                if( !net_point.hole )
                    net_point.drill_radius = 0;
                else
                    net_point.drill_radius = std::min( drill.x, drill.y );

                net_point.smd = pad->GetAttribute() == PAD_ATTRIB::SMD
                            || pad->GetAttribute() == PAD_ATTRIB::CONN;
                net_point.is_via = false;
                net_point.mechanical = ( pad->GetAttribute() == PAD_ATTRIB::NPTH );
                net_point.x_location = pad->GetPosition().x - origin.x;
                net_point.y_location = origin.y - pad->GetPosition().y;
                net_point.x_size = pad->GetSize().x;

                // Rule: round pads have y = 0
                if( pad->GetShape() == PAD_SHAPE::CIRCLE )
                    net_point.y_size = net_point.x_size;
                else
                    net_point.y_size = pad->GetSize().y;

                net_point.rotation = - pad->GetOrientation().AsDegrees();

                if( net_point.rotation < 0 )
                    net_point.rotation += 360;

                // always output Net end point as net test point
                net_point.epoint = "e";

                // the value indicates which sides are *not* accessible
                net_point.soldermask = 3;

                if( pad->GetLayerSet()[F_Mask] )
                    net_point.soldermask &= ~1;

                if( pad->GetLayerSet()[B_Mask] )
                    net_point.soldermask &= ~2;
                
                aRecords[pad->GetNetCode()].push_back( net_point );
            }
        }
    }
}

// Compute the side code for a via.
std::string ODB_NET_LIST::ComputeViaAccessSide( BOARD *aBoard, int top_layer, int bottom_layer )
{
    // Easy case for through vias: top_layer is component, bottom_layer is
    // solder, side code is Both
    if( (top_layer == F_Cu) && (bottom_layer == B_Cu) )
        return "B";

    // Blind via, reachable from front, Top
    if( top_layer == F_Cu )
        return "T";

    // Blind via, reachable from bottom, Down
    if( bottom_layer == B_Cu )
        return "D";

    // It's a buried via, accessible from some inner layer, Inner
    return "I";
}


void ODB_NET_LIST::InitViaNetPoints( BOARD *aBoard, std::map<size_t, std::vector<ODB_NET_RECORD>>& aRecords )
{
    VECTOR2I origin = aBoard->GetDesignSettings().GetAuxOrigin();

    // Enumerate all the track segments and keep the vias
    for( auto track : aBoard->Tracks() )
    {
        if( track->Type() == PCB_VIA_T )
        {
            PCB_VIA *via = static_cast<PCB_VIA*>( track );
            PCB_LAYER_ID top_layer, bottom_layer;

            via->LayerPair( &top_layer, &bottom_layer );

            ODB_NET_RECORD net_point;
            net_point.side = ComputeViaAccessSide( aBoard, top_layer, bottom_layer );
            
            if( net_point.side != "I" )
            {
                NETINFO_ITEM *net = track->GetNet();
                net_point.smd = false;
                net_point.hole = true;
                if( net->GetNetCode() == 0 )
                    net_point.netname = "$NONE$";
                else
                    net_point.netname = net->GetNetname();

                net_point.refdes = "VIA";
                net_point.is_via = true;
                net_point.drill_radius = via->GetDrillValue();
                net_point.mechanical = false;
                net_point.x_location = via->GetPosition().x - origin.x;
                net_point.y_location = origin.y - via->GetPosition().y;

                // via always has drill radius, Width and Height are 0
                net_point.x_size = 0;
                net_point.y_size = 0; // Round so height = 0
                net_point.rotation = 0;
                net_point.epoint = "e";  // only buried via is "m" net mid point
                
                // the value indicates which sides are *not* accessible
                net_point.soldermask = 3;

                if( via->GetLayerSet()[F_Mask] )
                    net_point.soldermask &= ~1;

                if( via->GetLayerSet()[B_Mask] )
                    net_point.soldermask &= ~2;

                aRecords[net->GetNetCode()].push_back( net_point );
            }
        }
    }
}


/* Write all the accumuled data to the file in D356 format */
void ODB_NET_LIST::WriteNetPointRecords( std::map<size_t, std::vector<ODB_NET_RECORD>>& aRecords,
                                std::ostream& aStream )
{
    aStream << "H optimize n staggered n" << std::endl;
    for( const auto& [key, vec] : aRecords )
    {
        aStream << "$" << key << " " << ODB::GenODBString( vec.front().netname ) << std::endl;
    }
    
    aStream << "#" << std::endl
            << "#Netlist points" << std::endl
            << "#" << std::endl;

    for( const auto& [key, vec] : aRecords )
    {
        for( const auto& net_point : vec )
        {
            aStream << key << " " ;
            if( net_point.hole )
                aStream << ODB::Float2StrVal( m_ODBScale * net_point.drill_radius );
            else
                aStream << 0;

            aStream << " " << ODB::Float2StrVal( m_ODBScale * net_point.x_location ) << " "
                    << ODB::Float2StrVal( m_ODBScale * net_point.y_location ) << " "
                    << net_point.side << " ";
            
            if( !net_point.hole )
                aStream << ODB::Float2StrVal( m_ODBScale * net_point.x_size ) << " "
                        << ODB::Float2StrVal( m_ODBScale * net_point.y_size ) << " ";
            
            std::string exp;
            if( net_point.soldermask == 3 )
                exp = "c";
            else if( net_point.soldermask == 2 )
                exp = "s";
            else if( net_point.soldermask == 1 )
                exp = "p";
            else if( net_point.soldermask == 0 )
                exp = "e";            

            aStream << net_point.epoint << " " << exp << " " ;
            
            if( net_point.hole )
                aStream << " staggered 0 0 0" ;

            if( net_point.is_via )
                aStream << " v" ;
            
            aStream << std::endl;
        }
    }    
}


void ODB_NET_LIST::Write( std::ostream& aStream )
{
    std::map<size_t, std::vector<ODB_NET_RECORD>> net_point_records;

    InitViaNetPoints( m_board, net_point_records );

    InitPadNetPoints( m_board, net_point_records );

    WriteNetPointRecords( net_point_records, aStream );
}


