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

#ifndef _ODB_EDA_DATA_H_
#define _ODB_EDA_DATA_H_


#include <list>
#include <memory>

#include "odb_attribute.h"
#include "odb_feature.h"

// namespace horizon {
// class Net;
// class Pad;
// class Package;
// } // namespace horizon

// enum class ODB_FID_TYPE;

class PKG_OUTLINE;
class EDAData : public ATTR_MANAGER
{
public:
    EDAData();

    void Write(std::ostream &ost) const;
    unsigned int GetLyrIdx( const wxString& aLayerName );
    std::vector<std::shared_ptr<FOOTPRINT>> GetEdaFootprints() const { return m_eda_footprints; }
    
    class FeatureID
    {
        friend EDAData;

    public:
        enum class Type
        { 
            COPPER,
            LAMINATE,
            HOLE
        };

        FeatureID(Type t, unsigned int l, unsigned int fid) : type(t), layer(l), feature_id(fid)
        {
        }

        Type type;
        unsigned int layer;
        unsigned int feature_id;

        void Write(std::ostream &ost) const;
    };

    class Subnet
    {
    public:
        Subnet(unsigned int i, EDAData *eda) : index(i), m_edadata( eda )
        {
        }
        const unsigned int index;
        void Write(std::ostream &ost) const;

        std::list<FeatureID> feature_ids;
        void AddFeatureID( FeatureID::Type type,
                           const wxString &layer,
                           unsigned int feature_id );

        virtual ~Subnet()
        {
        }

    protected:
        virtual void write_subnet(std::ostream &ost) const = 0;
        EDAData *m_edadata;
    };

    class SubnetVia : public Subnet
    {
    public:
        SubnetVia( unsigned int i, EDAData* aEda ) : Subnet( i, aEda )
        {
        }
        void write_subnet(std::ostream &ost) const override;
    };

    class SubnetTrace : public Subnet
    {
    public:
        SubnetTrace( unsigned int i,  EDAData* aEda ) : Subnet( i, aEda )
        {
        }
        void write_subnet(std::ostream &ost) const override;
    };

    class SubnetPlane : public Subnet
    {
    public:
        enum class FillType { SOLID, OUTLINE };
        enum class CutoutType { CIRCLE, RECT, OCTAGON, EXACT };

        SubnetPlane( unsigned int i, EDAData* aEda, FillType ft, CutoutType ct, size_t fs )
            : Subnet( i, aEda ), fill_type(ft), cutout_type(ct), fill_size(fs)
        {
        }

        FillType fill_type;
        CutoutType cutout_type;
        size_t fill_size;

        void write_subnet(std::ostream &ost) const override;
    };

    class SubnetToeprint : public Subnet
    {
    public:
        enum class Side { TOP, BOTTOM };

        SubnetToeprint(unsigned int i, EDAData* aEda, Side s, unsigned int c, unsigned int t)
            : Subnet( i, aEda ), side(s), comp_num(c), toep_num(t)
        {
        }

        ~SubnetToeprint()
        {
        }

        Side side;

        unsigned int comp_num;
        unsigned int toep_num;

        void write_subnet(std::ostream &ost) const override;
    };

    class Net : public ATTR_RECORD_WRITER
    {
    public:
        // template <typename T> using check_type = attribute::is_net<T>;

        Net(unsigned int index, const wxString &name);
        const unsigned int index;

        wxString name;

        std::list<std::unique_ptr<Subnet>> subnets;

        template <typename T, typename... Args> T& AddSubnet( Args &&...args )
        {
            auto f = std::make_unique<T>( subnets.size(), std::forward<Args>(args)... );
            auto &r = *f;
            subnets.push_back( std::move( f ) );
            return r;
        }

        void Write( std::ostream &ost ) const;
    };

    void AddNET( const NETINFO_ITEM* aNet );
    Net& GetNet( size_t aNetcode )
    {
        return nets_map.at( aNetcode );
    }


    class Pin
    {
    public:
        Pin( const size_t aIndex, const wxString& aName )
            : m_index( aIndex ), m_name( aName ) {}
            
        const size_t m_index;
        wxString m_name;

        std::pair<wxString, wxString> m_center;

        enum class Type { THROUGH_HOLE, BLIND, SURFACE };
        Type type = Type::SURFACE;

        enum class ElectricalType { ELECTRICAL, MECHANICAL, UNDEFINED };
        ElectricalType etype = ElectricalType::UNDEFINED;

        enum class MountType
        {
            SMT,
            SMT_RECOMMENDED,
            THROUGH_HOLE,
            THROUGH_RECOMMENDED,
            PRESSFIT,
            NON_BOARD,
            HOLE,
            UNDEFINED
        };
        MountType mtype = MountType::UNDEFINED;

        std::list<std::unique_ptr<PKG_OUTLINE>> m_pinOutlines;

        void Write(std::ostream &ost) const;
    };

    class Package : public ATTR_RECORD_WRITER
    {
    public:
        // template <typename T> using check_type = attribute::is_pkg<T>;

        Package( const size_t aIndex, const wxString& afpName )
         : m_index( aIndex ), m_name( afpName ) {}
        
        const size_t m_index;   /// <! Reference number of the package to be used in CMP.
        wxString m_name;

        size_t m_pitch;
        int64_t m_xmin, m_ymin, m_xmax, m_ymax;  // Box points: leftlow, rightup

        std::list<std::unique_ptr<PKG_OUTLINE>> m_pkgOutlines;

        void AddPin( const PAD* aPad, size_t aPinNum );
        const std::shared_ptr<Pin> GetEdaPkgPin( size_t aPadIndex ) const
        {
            return m_pinsVec.at( aPadIndex );
        }

        void Write(std::ostream &ost) const;

    private:
        std::vector<std::shared_ptr<Pin>> m_pinsVec;
    };

    void AddPackage( const FOOTPRINT* aFp );
    const Package& GetPackage( size_t aHash ) const
    {
        return packages_map.at( aHash );
    }

private:
    std::map<size_t, Net> nets_map;
    std::list<const Net*> nets;

    std::map<size_t, Package> packages_map;    //hash value, package
    std::list<const Package*> packages;

    std::map<wxString, unsigned int> layers_map;
    std::vector<wxString> layers;
    std::vector<std::shared_ptr<FOOTPRINT>> m_eda_footprints;

};

class PKG_OUTLINE
{
public:
    virtual void Write(std::ostream &ost) const = 0;

    virtual ~PKG_OUTLINE() = default;
};

class OUTLINE_RECT : public PKG_OUTLINE
{
public:
    OUTLINE_RECT(const VECTOR2I& aLowerLeft, size_t aWidth, size_t aHeight )
        : m_lower_left( aLowerLeft ), m_width( aWidth ), m_height( aHeight )
    {
    }

    OUTLINE_RECT( const BOX2I &aBox )
        : OUTLINE_RECT( aBox.GetPosition(), aBox.GetWidth(), aBox.GetHeight() )
    {
    }

    VECTOR2I m_lower_left;
    size_t m_width;
    size_t m_height;

    void Write(std::ostream &ost) const override;
};

class ODB_SURFACE_DATA;
class OUTLINE_CONTOUR : public PKG_OUTLINE
{
public:
    OUTLINE_CONTOUR( const SHAPE_POLY_SET::POLYGON &aPolygon, FILL_T aFillType = FILL_T::FILLED_SHAPE )
    {
        if( !aPolygon.empty() && aPolygon[0].PointCount() >= 3 )
        {
            m_surfaces = std::make_unique<ODB_SURFACE_DATA>( aPolygon );
            if( aFillType != FILL_T::NO_FILL )
            {
                m_surfaces->AddPolygonHoles( aPolygon );
            }
        }
    }

    std::unique_ptr<ODB_SURFACE_DATA> m_surfaces;

    void Write(std::ostream &ost) const override;
};

class OUTLINE_SQUARE : public PKG_OUTLINE
{
public:
    OUTLINE_SQUARE( const VECTOR2I& aCenter, size_t aHalfSide ) : m_center( aCenter ), m_halfSide( aHalfSide )
    {
    }
    VECTOR2I m_center;
    size_t m_halfSide;

    void Write(std::ostream &ost) const override;
};

class OUTLINE_CIRCLE : public PKG_OUTLINE
{
public:
    OUTLINE_CIRCLE( const VECTOR2I& aCenter, size_t aRadius )
     : m_center( aCenter ), m_radius( aRadius )
    {
    }
    VECTOR2I m_center;
    size_t m_radius;

    void Write(std::ostream &ost) const override;
};



#endif // _ODB_EDA_DATA_H_