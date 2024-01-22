#include "odb_attribute.h"
#include "pad.h"
#include "footprint.h"
#include <list>


enum class ODB_DIRECTION
{
    CW,
    CCW
};


// struct ODB_ROUNDRECT
// {
//     int width;   
//     int height;
//     int rad;
//     wxString corners;  

//     ODB_ROUNDRECT( int aWidth, int aHeight, int aRad, const wxString& aCorners ) : 
//         width( aWidth ), height( aHeight ), rad( aRad ), corners( aCorners )
//     {
//     }
// };


class FEATURES_MANAGER : public AttributeProvider
{
public:
    FEATURES_MANAGER( BOARD* aBoard ) : m_board( aBoard )
    {
    }

    virtual ~FEATURES_MANAGER() = default;

    void InitFeatureList( PCB_LAYER_ID aLayer,
                          std::vector<BOARD_ITEM*>& aItems );

    ODB_LINE& AddFeatureLine( const VECTOR2I& aStart,
                          const VECTOR2I& aEnd, uint64_t aWidth);

    ODB_ARC& AddFeatureArc( const VECTOR2I& aStart, const VECTOR2I& aEnd,
                        const VECTOR2I& aCenter, uint64_t aWidth,
                        ODB_DIRECTION aDirection );

    // std::vector<ODB_FEATURE *> draw_polygon_outline(const Polygon &poly, const Placement &transform);

    ODB_PAD& AddFeaturePad( const VECTOR2I& aCenter, const wxString& aSym,
                            const EDA_ANGLE& aAngle, bool aMirror );

    ODB_PAD& AddPadCircle(  const VECTOR2I& aCenter, uint64_t aDiameter,
                            const EDA_ANGLE& aAngle, bool aMirror, double aResize = 1.0 );

    void AddPadShape( const PAD& aPad, PCB_LAYER_ID aLayer );

    ODB_SURFACE& AddFeatureSurface( const SHAPE_POLY_SET::POLYGON& aPolygon,
                                    FILL_T aFillType = FILL_T::FILLED_SHAPE );

    void AddShape( const PCB_SHAPE& aShape );

    bool AddContour( const SHAPE_POLY_SET& aPolySet, int aOutline = 0,
                     FILL_T aFillType = FILL_T::FILLED_SHAPE );

    bool AddPolygon( const SHAPE_POLY_SET::POLYGON& aPolygon,
                     FILL_T aFillType, int aWidth, LINE_STYLE aDashType );

    bool AddPolygonCutouts( const SHAPE_POLY_SET::POLYGON& aPolygon );


    void GenerateFeatureFile( std::ostream &ost ) const;

private:
    inline uint32_t AddCircleSymbol( const wxString& aDiameter)
    {
        return GetSymbolIndex( m_circleSymMap, aDiameter );
    }

    inline uint32_t AddPadSymbol( const wxString& aPad )
    {
        return GetSymbolIndex( m_padSymMap, aPad );
    }
    
    inline uint32_t AddRectSymbol( const wxString& aWidth, const wxString& aHeight )
    {
        return GetSymbolIndex( m_rectSymMap, std::make_pair( aWidth, aHeight ) );
    }
    
    inline uint32_t AddOvalSymbol( const wxString& aWidth, const wxString& aHeight )
    {
        return GetSymbolIndex( m_ovalSymMap, std::make_pair( aWidth, aHeight ) );
    }

    inline uint32_t AddRoundRectSymbol( const wxString& aRoundRectDim )
    {
        return GetSymbolIndex( m_roundRectSymMap, aRoundRectDim );
    }

    inline uint32_t AddChamferRectSymbol( const wxString& aChamferRectDim )
    {
        return GetSymbolIndex( m_chamRectSymMap, aChamferRectDim );
    }
    


    template <typename T>
    uint32_t GetSymbolIndex( std::map<T, uint32_t>& aSymMap, const T& aKey )
    {
        if ( aSymMap.count( aKey ) )
        {
            return aSymMap.at( aKey );
        }
        else
        {
            uint32_t index = m_symIndex++;
            aSymMap.emplace( aKey, index );
            return index;
        }
    }

    std::map<wxString, uint32_t> m_circleSymMap;                    // diameter -> symbol index
    std::map<wxString, uint32_t> m_padSymMap;                    // name -> symbol index
    std::map<std::pair<wxString, wxString>, uint32_t> m_rectSymMap; // w,h -> symbol index
    std::map<std::pair<wxString, wxString>, uint32_t> m_ovalSymMap; // w,h -> symbol index
    std::map<wxString, uint32_t> m_roundRectSymMap;
    std::map<wxString, uint32_t> m_chamRectSymMap;

    template <typename T, typename... Args>
    T& AddFeature( Args&&... args )
    {
        T* feature = std::make_unique<T>( m_featuresList.size(),
                                          std::forward<Args>( args )... );
        
        if( feature != nullptr )
        {
            T& ref = *feature;
            m_featuresList.push_back( std::move( feature ) );
            return ref;
        }

        return *nullptr;

    }



    BOARD*   m_board;
    uint32_t m_symIndex = 0;

    std::list<std::unique_ptr<ODB_FEATURE>> m_featuresList;
};




class ODB_FEATURE : public ATTR_RECORD_WRITER
{
public:
    template <typename T> using check_type = attribute::is_feature<T>;

    virtual void WriteFeatures( std::ostream &ost );

    virtual ~ODB_FEATURE() = default;

protected:
    ODB_FEATURE( uint32_t aIndex ) : m_index( aIndex ) {}    

    enum class FEATURE_TYPE
    { 
        LINE,
        ARC,
        PAD,
        SURFACE
    };

    virtual FEATURE_TYPE GetFeatureType() const = 0;

    virtual void WriteRecordContent( std::ostream &ost ) = 0;

    const uint32_t m_index;
};

class ODB_LINE : public ODB_FEATURE
{
public:

    ODB_LINE( uint32_t aIndex, const std::pair<wxString, wxString>& aStart,
              const std::pair<wxString, wxString>& aEnd, uint32_t aSym)
              : ODB_FEATURE( aIndex ), m_start( aStart ), m_end( aEnd ),
              m_symIndex(aSym)
    {
    }

    inline virtual FEATURE_TYPE GetFeatureType() const override
    {
        return FEATURE_TYPE::LINE;
    }

protected:
    virtual void WriteRecordContent( std::ostream &ost ) override;

private:
    std::pair<wxString, wxString> m_start;
    std::pair<wxString, wxString> m_end;
    uint32_t m_symIndex;

};


class ODB_ARC : public ODB_FEATURE
{
public:

    inline virtual FEATURE_TYPE GetFeatureType() const override
    {
        return FEATURE_TYPE::ARC;
    }

    

    ODB_ARC( uint32_t aIndex, const std::pair<wxString, wxString>& aStart,
             const std::pair<wxString, wxString>& aEnd,
             const std::pair<wxString, wxString>& aCenter,
             uint32_t aSym, ODB_DIRECTION aDirection )
        : ODB_FEATURE( aIndex ), m_start( aStart ), m_end( aEnd ),
        m_center( aCenter ), m_symIndex( aSym ), m_direction( aDirection )
    {
    }

protected:
    virtual void WriteRecordContent( std::ostream &ost ) override;

private:
    std::pair<wxString, wxString> m_start;
    std::pair<wxString, wxString> m_end;
    std::pair<wxString, wxString> m_center;
    uint32_t m_symIndex;
    ODB_DIRECTION m_direction;

};

class ODB_PAD : public ODB_FEATURE
{
public:
    ODB_PAD( uint32_t aIndex, const std::pair<wxString, wxString>& aCenter,
             uint32_t aSym, EDA_ANGLE aAngle = EDA_ANGLE::m_Angle0,
             bool aMirror = false, double aResize = 1.0 )
        : ODB_FEATURE( aIndex ), m_center( aCenter ), m_symIndex( aSym ),
        m_angle( aAngle ), m_mirror( aMirror ), m_resize( aResize )
    {
    }

    inline virtual FEATURE_TYPE GetFeatureType() const override
    {
        return FEATURE_TYPE::PAD;
    }

protected:
    virtual void WriteRecordContent( std::ostream &ost ) override;

private:
    std::pair<wxString, wxString> m_center;
    uint32_t m_symIndex;
    EDA_ANGLE m_angle;
    bool m_mirror;
    double m_resize;
};

class ODB_SURFACE : public ODB_FEATURE
{
public:
    ODB_SURFACE( uint32_t aIndex, const SHAPE_POLY_SET::POLYGON& aPolygon,
                 FILL_T aFillType = FILL_T::FILLED_SHAPE );

    virtual ~ODB_SURFACE() = default;

    virtual void WriteFeatures( std::ostream &ost ) override;

    inline virtual FEATURE_TYPE GetFeatureType() const override
    {
        return FEATURE_TYPE::SURFACE;
    }

    std::unique_ptr<ODB_SURFACE_DATA> m_surfaces;

protected:
    virtual void WriteRecordContent( std::ostream &ost ) override;
};


class ODB_SURFACE_DATA
{
public:
    ODB_SURFACE_DATA( const SHAPE_POLY_SET::POLYGON& aPolygon );

    struct SURFACE_LINE
    {
        enum class LINE_TYPE
        {
            SEGMENT,
            ARC
        };

        SURFACE_LINE( const VECTOR2I& aEnd ) : m_end( aEnd )
        {
        }

        SURFACE_LINE( const VECTOR2I& aEnd,
                      const VECTOR2I& aCenter,
                      ODB_DIRECTION aDirection )
                    : m_end( aEnd ), m_type( LINE_TYPE::ARC ),
                    m_center( aCenter ), m_direction( aDirection )
        {
        }

        VECTOR2I m_end;
        LINE_TYPE m_type = LINE_TYPE::SEGMENT;

        VECTOR2I m_center;
        ODB_DIRECTION m_direction;
    };

    bool AddPolygonHoles( const SHAPE_POLY_SET::POLYGON& aPolygon );
    void WriteData( std::ostream &ost ) const;

    // void append_polygon(const Polygon &poly, const Placement &transform = Placement());
    // void append_polygon_auto_orientation(const Polygon &poly, const Placement &transform = Placement());


    // first one is contour (island) oriented clockwise
    // remainder are holes oriented counter clockwise
    std::vector<std::vector<SURFACE_LINE>> m_polygons;
};


// class ODB_TEXT : public ODB_FEATURE
// {
// public:

//     ODB_TEXT( uint32_t aIndex, const std::pair<wxString, wxString>& aStart,
//               const std::pair<wxString, wxString>& aEnd, uint32_t aSym)
//               : ODB_FEATURE( aIndex ), m_start( aStart ), m_end( aEnd ),
//               m_symIndex(aSym)
//     {
//     }

//     inline virtual FEATURE_TYPE GetFeatureType() const override
//     {
//         return FEATURE_TYPE::LINE;
//     }

// protected:
//     virtual void WriteRecordContent( std::ostream &ost ) const override;

// private:
//     std::pair<wxString, wxString> m_start;
//     std::pair<wxString, wxString> m_end;
//     uint32_t m_symIndex;

// };
