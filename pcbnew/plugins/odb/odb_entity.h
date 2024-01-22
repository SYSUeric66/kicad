
#include "odb_util.h"
#include <optional>
#include <vector>
#include <map>
#include <wx/string.h>
#include "odb_attribute.h"
#include <iostream>
#include "odb_feature.h"
#include <functional>


 
class BOARD;
enum class Polarity
{
    POSITIVE,
    NEGATIVE
};

class ODB_ENTITY_BASE
{
public:
	ODB_ENTITY_BASE( BOARD* aBoard ) : m_board( aBoard ) {}

    ODB_ENTITY_BASE() : m_board( nullptr ) {}

	virtual ~ODB_ENTITY_BASE() = default;
    virtual bool GenerateFiles( ODB_TREE_WRITER& writer ) = 0;
    virtual bool CreateDirectiryTree( ODB_TREE_WRITER& writer );
    virtual std::string GetEntityName() = 0;
    virtual void InitEntityData() {}


//     ODB_GENERATE_INTERFACE* GetPluginInterface() const { return m_generator; }
//     ODB_BOARD_MANAGER* GetBoardManager() const { return m_boardManager; }
protected:
    BOARD*                        m_board;
    // std::string                   m_entityName;
    std::vector<std::string>      m_fileName;
//     ODB_BOARD_MANAGER* m_boardManager;

};

class ODB_MATRIX_ENTITY : public ODB_ENTITY_BASE
{
public:
	ODB_MATRIX_ENTITY( BOARD* aBoard ) : ODB_ENTITY_BASE( aBoard )
    {
    }
	virtual ~ODB_MATRIX_ENTITY();

    inline virtual std::string GetEntityName()
    {
        return "matrix";
    }

    struct MATRIX_LAYER
    {

        enum class CONTEXT
        { 
            BOARD,
            MISC
        };

        enum class TYPE {
            SIGNAL,
            POWER_GROUND,
            DIELECTRIC,
            MIXED,
            SOLDER_MASK,
            SOLDER_PASTE,
            SILK_SCREEN,
            DRILL,
            ROUT,
            DOCUMENT,
            COMPONENT,
            MASK,
            CONDUCTIVE_PASTE,
        };

        struct Span {
            wxString start;
            wxString end;
        };

        enum class Subtype {
            COVERLAY,
            COVERCOAT,
            STIFFENER,
            BEND_AREA,
            FLEX_AREA,
            RIGID_AREA,
            PSA,
            SILVER_MASK,
            CARBON_MASK,
        };

        std::optional<Span> m_span;
        std::optional<Subtype> m_addType;


        Polarity m_polarity = Polarity::POSITIVE;

        const uint32_t m_rowNumber;
        const std::string m_layerName;
        CONTEXT m_context;
        TYPE m_type;


        MATRIX_LAYER( uint32_t aRow, const std::string &aLayerName )
                    : m_rowNumber( aRow ), m_layerName( aLayerName )
        {
        }

    };


    void AddLayer( const wxString &aLayerName );
    void AddStep( const wxString &aStepName );


    bool GenerateMatrixFile( ODB_TREE_WRITER& writer );
    virtual bool GenerateFiles( ODB_TREE_WRITER& writer );
    virtual void InitEntityData();

private:

    std::map<wxString, unsigned int> m_matrixSteps;
    std::vector<MATRIX_LAYER> m_matrixLayers;  
    unsigned int m_row = 1;
    unsigned int m_col = 1;

};

class ODB_MISC_ENTITY : public ODB_ENTITY_BASE
{
public:
    ODB_MISC_ENTITY( BOARD* aBoard, const std::vector<wxString>& aValue )
     : ODB_ENTITY_BASE( aBoard )
    {
    }
    virtual ~ODB_MISC_ENTITY();

    inline virtual std::string GetEntityName()
    {
        return "misc";
    }

    bool AddAttrList();
    bool AddSysAttrFiles();
    virtual bool GenerateInfoFile( ODB_TREE_WRITER& writer );

    virtual bool GenerateFiles( ODB_TREE_WRITER& writer );
    
private:
    std::map<wxString, wxString> m_info;
    // ODB_ATTRLIST m_attrlist;

};

class EDAData;
class BoardPackage;
class ODB_STEP_ENTITY : public ODB_ENTITY_BASE
{
public:
    ODB_STEP_ENTITY( BOARD* aBoard );
    virtual ~ODB_STEP_ENTITY();

    inline virtual std::string GetEntityName()
    {
        return "pcb";
    }

    bool AddAttrList();
    bool AddBom();
    bool AddEdaData();
    bool InitLayerEntityData();
    bool AddNetList();
    bool AddProfile();
    bool AddStepHeader();
    // bool AddImpedanceFile();
    // bool AddZonesFile();
    virtual bool CreateDirectiryTree( ODB_TREE_WRITER& writer );
    virtual void InitEntityData();
    bool GenerateLayerFiles( ODB_TREE_WRITER& writer );
    bool GenerateEdaFiles( ODB_TREE_WRITER& writer ) {}
    bool GenerateNetlistsFiles( ODB_TREE_WRITER& writer ) {}
    bool GenerateProfileFile( ODB_TREE_WRITER& writer ) {}
    bool GenerateStepHeaderFile( ODB_TREE_WRITER& writer ) {}
    bool GenerateAttrListFile( ODB_TREE_WRITER& writer ) {}
    virtual bool GenerateFiles( ODB_TREE_WRITER& writer ) {}

private:

    // ODB_ATTRLIST m_attrList;
    std::map<wxString, std::shared_ptr<ODB_LAYER_ENTITY>> m_layerEntityMap;
    std::optional<ODB_FEATURE> m_profile;
    // std::optional<ODB_BOMS> m_boms;

    // EDAData m_edaData;
    STEP_HDR m_stepHdr;

};


class ODB_LAYER_ENTITY : public ODB_ENTITY_BASE
{
public:
    ODB_LAYER_ENTITY( BOARD* aBoard,
                      std::map<int, std::vector<BOARD_ITEM*>>& aMap,
                      const PCB_LAYER_ID& aLayerID );
                    
    virtual ~ODB_LAYER_ENTITY();

    inline virtual std::string GetEntityName()
    {
        return "layers";
    }

    void InitEntityData();
    
    void AddLayerFeatures();


    bool GenAttrList() { return true; }
    bool GenComponents() { return true; }
    bool GenTools() { return true; }

    bool GenFeatures( ODB_TREE_WRITER &writer );

    virtual bool GenerateFiles( ODB_TREE_WRITER& writer );

private:
    std::map<int, std::vector<BOARD_ITEM*>> m_layerItems;
    PCB_LAYER_ID m_layerID;
    // ODB_ATTRLIST m_attrList;
    // std::optional<ODB_COMPONENTS> m_compTop;
    // std::optional<ODB_COMPONENTS> m_compBot;
    std::unique_ptr<FEATURES_MANAGER> m_featuresMgr;
    std::optional<std::unique_ptr<FEATURES_MANAGER>> m_profile;


};

class ODB_SYMBOLS_ENTITY : public ODB_ENTITY_BASE
{
public:
    ODB_SYMBOLS_ENTITY( BOARD* aBoard ) : ODB_ENTITY_BASE( aBoard )
    {
    }
    virtual ~ODB_SYMBOLS_ENTITY();

    inline virtual std::string GetEntityName()
    {
        return "symbols";
    }

    virtual bool GenerateFiles( ODB_TREE_WRITER& writer );


};

class ODB_FONTS_ENTITY : public ODB_ENTITY_BASE
{
public:
    ODB_FONTS_ENTITY();
    virtual ~ODB_FONTS_ENTITY();

    inline virtual std::string GetEntityName()
    {
        return "fonts";
    }
    virtual bool GenerateFiles( ODB_TREE_WRITER& writer );


};

class ODB_WHEELS_ENTITY : public ODB_ENTITY_BASE
{
public:
    ODB_WHEELS_ENTITY();
    virtual ~ODB_WHEELS_ENTITY();

    inline virtual std::string GetEntityName()
    {
        return "wheels";
    }

    virtual bool GenerateFiles( ODB_TREE_WRITER& writer );


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

    virtual bool GenerateFiles( ODB_TREE_WRITER &writer );


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

    virtual bool GenerateFiles( ODB_TREE_WRITER &writer );


};

