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


#ifndef _PCB_IO_ODBPP_H_
#define _PCB_IO_ODBPP_H_

#include <pcb_io/pcb_io.h>
#include <pcb_io/pcb_io_mgr.h>
#include <pcb_io/common/plugin_common_layer_mapping.h>

#include <eda_shape.h>
#include <layer_ids.h> // PCB_LAYER_ID
#include <font/font.h>
#include <geometry/shape_segment.h>
#include <stroke_params.h>
#include <memory>
#include "odb_entity.h"

class BOARD;
class BOARD_ITEM;
class EDA_TEXT;
class FOOTPRINT;
class PROGRESS_REPORTER;
class NETINFO_ITEM;
class PAD;
class PCB_SHAPE;
class PCB_VIA;
class PROGRESS_REPORTER;
class SHAPE_POLY_SET;
class SHAPE_SEGMENT;
class EDAData::Subnet;





class PCB_IO_ODBPP : public PCB_IO, public LAYER_REMAPPABLE_PLUGIN
{
public:
    /**
     * @brief PCB_IO_ODBPP
     *
    */
    PCB_IO_ODBPP() : PCB_IO( wxS( "ODBPlusPlus" ) )
    {
        // m_show_layer_mapping_warnings = false;
        // m_total_bytes = 0;
        m_sigfig = 4;
        // m_version = 'B';
        // m_enterpriseNode = nullptr;
        m_board = nullptr;
        // m_props = nullptr;
        // m_shape_user_node = nullptr;
        // m_shape_std_node = nullptr;
        // m_line_node = nullptr;
        // m_last_padstack = nullptr;
        // m_progress_reporter = nullptr;
        // m_xml_doc = nullptr;
        // m_xml_root = nullptr;
    }

    ~PCB_IO_ODBPP() override;

    void SaveBoard( const wxString& aFileName, BOARD* aBoard,
                const STRING_UTF8_MAP* aProperties = nullptr ) override;

    const IO_BASE::IO_FILE_DESC GetBoardFileDesc() const override
    {
        return IO_BASE::IO_FILE_DESC( _HKI( "ODB++ Production File" ), { "ZIP" } );
    }

    const IO_BASE::IO_FILE_DESC GetLibraryDesc() const override
    {
        // No library description for this plugin
        return IO_BASE::IO_FILE_DESC( wxEmptyString, {} );
    }


    std::vector<FOOTPRINT*> GetImportedCachedLibraryFootprints() override;

    long long GetLibraryTimestamp( const wxString& aLibraryPath ) const override
    {
        return 0;
    }

    // Reading currently disabled
    bool CanReadBoard( const wxString& aFileName ) const override
    {
        return false;
    }

    // Reading currently disabled
    bool CanReadFootprint( const wxString& aFileName ) const override
    {
        return false;
    }

    // Reading currently disabled
    bool CanReadLibrary( const wxString& aFileName ) const override
    {
        return false;
    }

    // void add_matrix_layer(const std::string &name);
    // ODB_STEP_ENTITY &add_step(const std::string &name);
    // void Write( std::shared_ptr<ODB_TREE_WRITER> aTreeWriter ) const;


    /**
     * Return the automapped layers.
     *
     * @param aInputLayerDescriptionVector
     * @return Auto-mapped layers
     */
    // static std::map<wxString, PCB_LAYER_ID> DefaultLayerMappingCallback(
    //         const std::vector<INPUT_LAYER_DESC>& aInputLayerDescriptionVector );

    /**
     * Register a different handler to be called when mapping of IPC2581 to KiCad layers occurs.
     *
     * @param aLayerMappingHandler
     */
    // void RegisterLayerMappingCallback( LAYER_MAPPING_HANDLER aLayerMappingHandler ) override
    // {};

public:

    // struct ODB_JOB
    // {
    //     ODB_MATRIX_ENTITY m_matrix;
    //     ODB_MISC_ENTITY m_misc;
    //     ODB_FONTS_ENTITY m_fonts;
    //     ODB_SYMBOLS_ENTITY m_symbols;
    //     ODB_WHEELS_ENTITY m_wheels;

    //     std::map<wxString, ODB_STEP_ENTITY> m_steps;
    //     wxString m_jobName;

    //     // using SymbolKey = std::tuple<UUID, int, std::string>; // padstack UUID, layer, content hash
    //     // std::map<SymbolKey, Symbol> symbols;
    //     // std::set<std::string> symbol_names;

    // };
    inline std::vector<std::pair<PCB_LAYER_ID, wxString>>& GetLayerNameList()
    {
        return m_layer_name_list;
    }

    inline std::map<PCB_LAYER_ID, std::map<int, std::vector<BOARD_ITEM*>>>&
           GetLayerElementsMap()
    {
        return m_layer_elements;
    }

    inline std::vector<std::shared_ptr<FOOTPRINT>>& GetLoadedFootprintList()
    {
        return m_loaded_footprints;
    }

    inline std::map<std::pair<PCB_LAYER_ID, PCB_LAYER_ID>, std::vector<BOARD_ITEM*>>&
           GetDrillLayerItemsMap()
    {
        return m_drill_layers;
    }
    
    inline std::map<std::pair<PCB_LAYER_ID, PCB_LAYER_ID>, std::vector<PAD*>>&
           GetSlotHolesMap()
    {
        return m_slot_holes;
    }

    inline std::map<PAD*, EDAData::SubnetToeprint*>&
           GetPadSubnetMap()
    {
        return m_topeprint_subnets;
    }

    inline std::map<std::pair<PCB_LAYER_ID, ZONE*>, EDAData::SubnetPlane*>&
           GetPlaneSubnetMap()
    {
        return m_plane_subnets;
    }

    inline std::map<PCB_TRACK*, EDAData::Subnet*>&
           GetViaTraceSubnetMap()
    {
        return m_via_trace_subnets;
    }


    std::shared_ptr<ODB_TREE_WRITER> m_writer;

    // inline const double GetODBScale() const { return m_ODBScale; }
    bool GenerateFiles( ODB_TREE_WRITER& writer );
    bool ExportODB( const wxString& aFileName );
    bool CreateEntity( ODB_TREE_WRITER& writer );
    void InitEntityData();
    
    bool CreateDirectories( ODB_TREE_WRITER& writer );
    /**
     * Frees the memory allocated for the loaded footprints in #m_loaded_footprints.
     *
    */
    void ClearLoadedFootprints();

private:

    template <typename T,  typename... Args>
    void Make( Args&&... args )
    {
        std::shared_ptr<ODB_ENTITY_BASE> entity = 
                std::make_shared<T>( std::forward<Args>( args )... );

        if( entity )
            m_entities.push_back( entity );
    }
    // LAYER_MAPPING_HANDLER   m_layerMappingHandler;
    // bool                    m_show_layer_mapping_warnings;

    // size_t                  m_total_bytes;  //<! Total number of bytes to be written

    wxString                m_units_str;    //<! Output string for units
    int                     m_sigfig;       //<! Max number of digits past the decimal point
    // // char                    m_version;      //<! Currently, either 'B' or 'C' for the IPC2581 version
    // wxString                m_OEMRef;       //<! If set, field name containing the internal ID of parts
    // wxString                m_mpn;          //<! If set, field name containing the manufacturer part number
    // wxString                m_mfg;          //<! If set, field name containing the part manufacturer
    // wxString                m_distpn;       //<! If set, field name containing the distributor part number
    // wxString                m_dist;         //<! If set, field name containing the distributor name


    BOARD*                  m_board;
    std::vector<std::shared_ptr<FOOTPRINT>> m_loaded_footprints;
    // std::vector<FOOTPRINT*> m_loaded_footprints;
    // const STRING_UTF8_MAP*  m_props;

    // std::map<size_t, wxString> m_user_shape_dict;   //<! Map between shape hash values and reference id string

    // std::map<size_t, wxString> m_std_shape_dict;    //<! Map between shape hash values and reference id string

    // std::map<size_t, wxString> m_line_dict;         //<! Map between line hash values and reference id string

    // std::map<size_t, wxString> m_padstack_dict;     //<! Map between padstack hash values and reference id string (PADSTACK_##)
 
    
    // std::map<size_t, wxString>
    //         m_footprint_dict; //<! Map between the footprint hash values and reference id string (<fpid>_##)

    // std::map<FOOTPRINT*, wxString>
    //         m_OEMRef_dict; //<! Reverse map from the footprint pointer to the reference id string assigned for components

    // std::map<int, std::vector<std::pair<wxString, wxString>>>
    //         m_net_pin_dict; //<! Map from netcode to the component/pin pairs in the net

    std::vector<std::pair<PCB_LAYER_ID, wxString>>
            m_layer_name_list; //<! layer name in matrix entity to the internal layer id

    std::map<std::pair<PCB_LAYER_ID, PCB_LAYER_ID>, std::vector<BOARD_ITEM*>>
            m_drill_layers; //<! Drill sets are output as layers (to/from pairs)

    std::map<std::pair<PCB_LAYER_ID, PCB_LAYER_ID>, std::vector<PAD*>>
            m_slot_holes; //<! Storage vector of slotted holes that need to be output as cutouts
    
    std::map<PCB_LAYER_ID, std::map<int, std::vector<BOARD_ITEM*>>> 
            m_layer_elements; //<! Storage map of layer to element list

    std::map<PAD*, EDAData::SubnetToeprint*>
            m_topeprint_subnets;
            
    std::map<std::pair<PCB_LAYER_ID, ZONE*>, EDAData::SubnetPlane*>
            m_plane_subnets;

    std::map<PCB_TRACK*, EDAData::Subnet*>
            m_via_trace_subnets;

    std::vector<std::shared_ptr<ODB_ENTITY_BASE>> m_entities;
    // PROGRESS_REPORTER*      m_progress_reporter;

    // std::set<wxUniChar>     m_acceptable_chars;     //<! IPC2581B and C have differing sets of allowed characters in names

};

#endif // _PCB_IO_ODBPP_H_