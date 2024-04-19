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

#ifndef _ODB_ENTITY_H_
#define _ODB_ENTITY_H_


#include <optional>
#include <vector>
#include <map>
#include <wx/string.h>
#include <iostream>
#include <functional>
#include "odb_feature.h"
#include "odb_eda_data.h"
#include "odb_netlist.h"
#include "odb_component.h"


class BOARD;
class ODB_TREE_WRITER;
class BOARD_ITEM;
class ODB_PLUGIN;

class ODB_ENTITY_BASE
{
public:
	ODB_ENTITY_BASE( BOARD* aBoard, ODB_PLUGIN* aPlugin )
        : m_board( aBoard ), m_plugin( aPlugin ) {}

    ODB_ENTITY_BASE() : m_board( nullptr ), m_plugin( nullptr ) {}

	virtual ~ODB_ENTITY_BASE() = default;
    virtual bool GenerateFiles( ODB_TREE_WRITER& writer ) { return true; }
    virtual bool CreateDirectiryTree( ODB_TREE_WRITER& writer );
    virtual std::string GetEntityName() = 0;
    virtual void InitEntityData() {}


//     ODB_GENERATE_INTERFACE* GetPluginInterface() const { return m_generator; }
//     ODB_BOARD_MANAGER* GetBoardManager() const { return m_boardManager; }
protected:
    BOARD*                        m_board;
    // std::string                   m_entityName;
    std::vector<std::string>      m_fileName;
    ODB_PLUGIN*                   m_plugin;
//     ODB_BOARD_MANAGER* m_boardManager;

};

enum class ODB_SUBTYPE;
enum class ODB_POLARITY;
enum class ODB_CONTEXT;
enum class ODB_TYPE;

class ODB_MATRIX_ENTITY : public ODB_ENTITY_BASE
{
public:
	ODB_MATRIX_ENTITY( BOARD* aBoard, ODB_PLUGIN* aPlugin )
         : ODB_ENTITY_BASE( aBoard, aPlugin ) {}

	virtual ~ODB_MATRIX_ENTITY() = default;

    inline virtual std::string GetEntityName()
    {
        return "matrix";
    }

    struct MATRIX_LAYER
    {
        std::optional<std::pair<wxString, wxString>> m_span;   // !< start, end
        std::optional<ODB_SUBTYPE> m_addType;
        std::optional<ODB_DIELECTRIC_TYPE> m_diType;
        
        uint32_t m_rowNumber;
        wxString m_layerName;
        ODB_CONTEXT m_context;
        ODB_TYPE m_type;
        ODB_POLARITY m_polarity = ODB_POLARITY::POSITIVE;

        MATRIX_LAYER( uint32_t aRow, const wxString& aLayerName )
                    : m_rowNumber( aRow ), m_layerName( aLayerName )
        {
            m_layerName.Replace( wxT( "." ), wxT( "_" ) );
            m_layerName.MakeUpper();
        }
    };


    bool GenerateMatrixFile( ODB_TREE_WRITER& writer );
    virtual bool GenerateFiles( ODB_TREE_WRITER& writer );
    virtual void InitEntityData();
    void InitMatrixLayerData();

    void AddStep( const wxString& aStepName );
    void AddMatrixLayerField( MATRIX_LAYER& aMLayer, PCB_LAYER_ID aLayer );
    void AddDrillMatrixLayer();
    void AddCOMPMatrixLayer();

private:

    std::map<wxString, unsigned int> m_matrixSteps;
    std::vector<MATRIX_LAYER> m_matrixLayers;
    unsigned int m_row = 1;
    unsigned int m_col = 1;
    bool m_hasBotComp = false;

};

class ODB_MISC_ENTITY : public ODB_ENTITY_BASE
{
public:
    ODB_MISC_ENTITY( const std::vector<wxString>& aValue );
    virtual ~ODB_MISC_ENTITY() = default;
    inline virtual std::string GetEntityName()
    {
        return "misc";
    }

    // bool AddAttrList();
    // bool AddSysAttrFiles();
    virtual bool GenerateInfoFile( ODB_TREE_WRITER& writer );

    virtual bool GenerateFiles( ODB_TREE_WRITER& writer );
    
private:
    std::map<wxString, wxString> m_info;
    // ODB_ATTRLIST m_attrlist;

};

class FEATURES_MANAGER;
class ODB_LAYER_ENTITY;
class ODB_STEP_ENTITY : public ODB_ENTITY_BASE
{
public:
    ODB_STEP_ENTITY( BOARD* aBoard, ODB_PLUGIN* aPlugin );
    virtual ~ODB_STEP_ENTITY() = default;

    inline virtual std::string GetEntityName()
    {
        return "pcb";
    }

    // bool AddAttrList();
    // bool AddBom();
    void InitEdaData();
    void InitPackage();
    void InitNetListData();
    void MakeLayerEntity();
    void InitLayerEntityData();
    bool AddNetList();
    bool AddProfile();
    bool AddStepHeader();
    // bool AddImpedanceFile();
    // bool AddZonesFile();
    virtual bool CreateDirectiryTree( ODB_TREE_WRITER& writer );
    
    virtual void InitEntityData();
    bool GenerateLayerFiles( ODB_TREE_WRITER& writer );
    bool GenerateEdaFiles( ODB_TREE_WRITER& writer );
    bool GenerateNetlistsFiles( ODB_TREE_WRITER& writer );
    bool GenerateProfileFile( ODB_TREE_WRITER& writer );
    bool GenerateStepHeaderFile( ODB_TREE_WRITER& writer );
    bool GenerateAttrListFile( ODB_TREE_WRITER& writer ) { return true; }
    virtual bool GenerateFiles( ODB_TREE_WRITER& writer );

private:

    // ODB_ATTRLIST m_attrList;
    std::map<wxString, std::shared_ptr<ODB_LAYER_ENTITY>> m_layerEntityMap;
    std::unique_ptr<FEATURES_MANAGER> m_profile;
    // std::optional<ODB_BOMS> m_boms;

    EDAData m_edaData;
    std::unordered_map<wxString, wxString> m_stephdr;
    ODB_NET_LIST m_netlist;
};

// class ODB_COMPONENT;
// class COMPONENTS_MANAGER;
class ODB_LAYER_ENTITY : public ODB_ENTITY_BASE
{
public:
    ODB_LAYER_ENTITY( BOARD* aBoard, ODB_PLUGIN* aPlugin,
                      std::map<int, std::vector<BOARD_ITEM*>>& aMap,
                      const PCB_LAYER_ID& aLayerID, const wxString& aLayerName );

    // ODB_LAYER_ENTITY( BOARD* aBoard, ODB_PLUGIN* aPlugin )
    //              : ODB_ENTITY_BASE( aBoard, aPlugin )
    // {
    //     m_featuresMgr = std::make_unique<FEATURES_MANAGER>( aBoard );
    // }
                    
    virtual ~ODB_LAYER_ENTITY() = default;

    inline virtual std::string GetEntityName()
    {
        return "layers";
    }
    virtual void InitEntityData();
    void InitFeatureData();
    ODB_COMPONENT& InitComponentData( FOOTPRINT* aFp, EDAData& aEDAData );
    void InitDrillData();
    void InitSlotData();
    
    void AddLayerFeatures();


    bool GenAttrList() { return true; }
    bool GenComponents( ODB_TREE_WRITER &writer );
    bool GenTools( ODB_TREE_WRITER &writer );

    bool GenFeatures( ODB_TREE_WRITER &writer );

    virtual bool GenerateFiles( ODB_TREE_WRITER& writer );

private:
    std::map<int, std::vector<BOARD_ITEM*>> m_layerItems;
    PCB_LAYER_ID m_layerID;
    wxString m_matrixLayerName;
    // ODB_ATTRLIST m_attrList;
    std::optional<ODB_DRILL_TOOLS> m_tools;
    std::optional<COMPONENTS_MANAGER> m_compTop;
    std::optional<COMPONENTS_MANAGER> m_compBot;
    std::unique_ptr<FEATURES_MANAGER> m_featuresMgr;
     std::map<size_t, wxString>
            m_footprint_dict; //<! Map between the footprint hash values and reference id string (<fpid>_##)



};

class ODB_SYMBOLS_ENTITY : public ODB_ENTITY_BASE
{
public:
    ODB_SYMBOLS_ENTITY() = default;
    
    virtual ~ODB_SYMBOLS_ENTITY() = default;

    inline virtual std::string GetEntityName()
    {
        return "symbols";
    }

    // virtual bool GenerateFiles( ODB_TREE_WRITER& writer );


};

class ODB_FONTS_ENTITY : public ODB_ENTITY_BASE
{
public:
    ODB_FONTS_ENTITY() = default;
    virtual ~ODB_FONTS_ENTITY() = default;

    inline virtual std::string GetEntityName()
    {
        return "fonts";
    }
    virtual bool GenerateFiles( ODB_TREE_WRITER& writer );

};

class ODB_WHEELS_ENTITY : public ODB_ENTITY_BASE
{
public:
    ODB_WHEELS_ENTITY() = default;
    virtual ~ODB_WHEELS_ENTITY() = default;

    inline virtual std::string GetEntityName()
    {
        return "wheels";
    }

    // virtual bool GenerateFiles( ODB_TREE_WRITER& writer ) { return true; }


};

class ODB_INPUT_ENTITY : public ODB_ENTITY_BASE
{
public:
    ODB_INPUT_ENTITY() = default;
    virtual ~ODB_INPUT_ENTITY() = default;

    inline virtual std::string GetEntityName()
    {
        return "input";
    }

    // virtual bool GenerateFiles( ODB_TREE_WRITER &writer ) { return true; }


};

class ODB_USER_ENTITY : public ODB_ENTITY_BASE
{
public:
    ODB_USER_ENTITY() = default;
    virtual ~ODB_USER_ENTITY() = default;

    inline virtual std::string GetEntityName()
    {
        return "user";
    }

    // virtual bool GenerateFiles( ODB_TREE_WRITER &writer ) { return true; }

};

#endif // _ODB_ENTITY_H_
