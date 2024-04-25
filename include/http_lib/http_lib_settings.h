/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2023 Andre F. K. Iwers <iwers11@gmail.com>
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

#ifndef KICAD_HTTP_LIB_SETTINGS_H
#define KICAD_HTTP_LIB_SETTINGS_H

#include <settings/json_settings.h>
#include <ctime>


enum class HTTP_LIB_SOURCE_TYPE
{
    REST_API,
    INVALID
};


struct HTTP_LIB_SOURCE
{
    HTTP_LIB_SOURCE_TYPE type;
    std::string          root_url;
    std::string          api_version;
    std::string          token;
    int                  timeout_parts;
    int                  timeout_categories;
};

struct HTTP_HQ_LIB_SOURCE : public HTTP_LIB_SOURCE
{
    // HTTP_LIB_SOURCE_TYPE type;
    // std::string          root_url;
    // std::string          api_version;
    // std::string          token;
    // int                  timeout_parts = 1000;
    // int                  timeout_categories = 1000;
    
    std::map<std::string, std::string> params;
};


struct HTTP_LIB_PART
{
    std::string id;
    std::string name;
    std::string symbolIdStr;

    bool exclude_from_bom = false;
    bool exclude_from_board = false;
    bool exclude_from_sim = false;

    std::time_t lastCached = 0;

    std::map<std::string, std::tuple<std::string, bool>> fields; ///< additional generic fields
};


struct HTTP_LIB_CATEGORY
{
    std::string id;   ///< id of category
    std::string name; ///< name of category

    std::time_t lastCached = 0;

    std::vector<HTTP_LIB_PART> cachedParts;
};


struct HTTP_HQ_PART
{
    std::string mpn;
    std::string manufacturerId;
    std::string pkg;
    std::string description;
    std::string datasheet;
    std::string createTime;  ///< to check if outdate
    std::string symbol_lib_name; //*.kicad_sym
    std::string fp_lib_name; // lib name inside the fp lib file, not used yet. *.kicad_mod
    std::string fp_lib_filename; // fp lib file name, so sym lib fp property: pretty_name:fp_lib_filename remove ".kicad_mod"
    std::string pretty_name = "kicad_community_lib"; ///<  *.pretty 

    std::time_t lastCached = 0;
    std::map<std::string, std::string> attrs;  // to override symbol lib properties
    std::map<std::string, std::string> fields; ///< additional generic fields
};

struct HTTP_HQ_CATEGORY
{
    std::string id;          ///< id of category
    std::string name;        ///< name of category
    std::string displayName; ///< display name of category
    std::string parentId;    ///< id of parent category
    std::string level;       ///< category level

    std::time_t lastCached = 0;

    std::vector<HTTP_HQ_PART> cachedParts;
};


class HTTP_LIB_SETTINGS : public JSON_SETTINGS
{
public:
    HTTP_LIB_SETTINGS( const std::string& aFilename );

    virtual ~HTTP_LIB_SETTINGS() {}

    HTTP_LIB_SOURCE m_Source;

    HTTP_LIB_SOURCE_TYPE get_HTTP_LIB_SOURCE_TYPE()
    {
        if( sourceType.compare( "REST_API" ) == 0 )
        {
            return HTTP_LIB_SOURCE_TYPE::REST_API;
        }

        return HTTP_LIB_SOURCE_TYPE::INVALID;
    }

    std::string getSupportedAPIVersion() { return api_version; }

protected:
    wxString getFileExt() const override;

private:
    std::string sourceType;
    std::string api_version = "v1";
};

#endif //KICAD_HTTP_LIB_SETTINGS_H
