/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2024 Jon Evans <jon@craftyjon.com>
 * Copyright (C) 2024 KiCad Developers, see AUTHORS.txt for contributors.
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

#include <fmt/format.h>
#include <wx/dir.h>
#include <wx/log.h>
#include <wx/utils.h>

#include <api/api_plugin_manager.h>
#include <api/api_server.h>
#include <paths.h>
#include <pgm_base.h>
#include <python_manager.h>
#include <settings/settings_manager.h>
#include <settings/common_settings.h>

const wxChar* const traceApi = wxT( "KICAD_API" );


wxDEFINE_EVENT( EDA_EVT_PLUGIN_MANAGER_JOB_FINISHED, wxCommandEvent );
wxDEFINE_EVENT( EDA_EVT_PLUGIN_AVAILABILITY_CHANGED, wxCommandEvent );


API_PLUGIN_MANAGER::API_PLUGIN_MANAGER( wxEvtHandler* aEvtHandler ) :
        wxEvtHandler(),
        m_parent( aEvtHandler )
{
    Bind( EDA_EVT_PLUGIN_MANAGER_JOB_FINISHED, &API_PLUGIN_MANAGER::processNextJob, this );
}


class PLUGIN_TRAVERSER : public wxDirTraverser
{
private:
    std::function<void( const wxFileName& )> m_action;

public:
    explicit PLUGIN_TRAVERSER( std::function<void( const wxFileName& )> aAction )
            : m_action( std::move( aAction ) )
    {
    }

    wxDirTraverseResult OnFile( const wxString& aFilePath ) override
    {
        wxFileName file( aFilePath );

        if( file.GetFullName() == wxS( "plugin.json" ) )
            m_action( file );

        return wxDIR_CONTINUE;
    }

    wxDirTraverseResult OnDir( const wxString& dirPath ) override
    {
        return wxDIR_CONTINUE;
    }
};


void API_PLUGIN_MANAGER::ReloadPlugins()
{
    m_plugins.clear();
    m_pluginsCache.clear();
    m_actionsCache.clear();
    m_environmentCache.clear();
    m_buttonBindings.clear();
    m_menuBindings.clear();
    m_readyPlugins.clear();

    // TODO support system-provided plugins
    wxDir userPluginsDir( PATHS::GetUserPluginsPath() );

    PLUGIN_TRAVERSER loader(
            [&]( const wxFileName& aFile )
            {
                wxLogTrace( traceApi, wxString::Format( "Manager: loading plugin from %s",
                                                        aFile.GetFullPath() ) );

                auto plugin = std::make_unique<API_PLUGIN>( aFile );

                if( plugin->IsOk() )
                {
                    if( m_pluginsCache.count( plugin->Identifier() ) )
                    {
                        wxLogTrace( traceApi,
                                    wxString::Format( "Manager: identifier %s already present!",
                                                      plugin->Identifier() ) );
                        return;
                    }
                    else
                    {
                        m_pluginsCache[plugin->Identifier()] = plugin.get();
                    }

                    for( const PLUGIN_ACTION& action : plugin->Actions() )
                        m_actionsCache[action.identifier] = &action;

                    m_plugins.insert( std::move( plugin ) );
                }
                else
                {
                    wxLogTrace( traceApi, "Manager: loading failed" );
                }
            } );

    if( userPluginsDir.IsOpened() )
    {
        wxLogTrace( traceApi, wxString::Format( "Manager: scanning user path (%s) for plugins...",
                                                userPluginsDir.GetName() ) );
        userPluginsDir.Traverse( loader );
        processPluginDependencies();
    }

    wxCommandEvent* evt = new wxCommandEvent( EDA_EVT_PLUGIN_AVAILABILITY_CHANGED, wxID_ANY );
    m_parent->QueueEvent( evt );
}


void API_PLUGIN_MANAGER::InvokeAction( const wxString& aIdentifier )
{
    if( !m_actionsCache.count( aIdentifier ) )
        return;

    const PLUGIN_ACTION* action = m_actionsCache.at( aIdentifier );
    const API_PLUGIN& plugin = action->plugin;

    if( !m_readyPlugins.count( plugin.Identifier() ) )
    {
        wxLogTrace( traceApi, wxString::Format( "Manager: Plugin %s is not ready",
                                                plugin.Identifier() ) );
        return;
    }

    wxFileName pluginFile( plugin.BasePath(), action->entrypoint );
    pluginFile.Normalize( wxPATH_NORM_ABSOLUTE | wxPATH_NORM_SHORTCUT | wxPATH_NORM_DOTS
                          | wxPATH_NORM_TILDE, plugin.BasePath() );
    wxString pluginPath = pluginFile.GetFullPath();

    std::vector<const wchar_t*> args;
    std::optional<wxString> py;

    switch( plugin.Runtime().type )
    {
    case PLUGIN_RUNTIME_TYPE::PYTHON:
    {
        py = PYTHON_MANAGER::GetVirtualPython( plugin.Identifier() );

        if( !py )
        {
            wxLogTrace( traceApi, wxString::Format( "Manager: Python interpreter for %s not found",
                                                    plugin.Identifier() ) );
            return;
        }

        args.push_back( py->wc_str() );

        if( !pluginFile.IsFileReadable() )
        {
            wxLogTrace( traceApi, wxString::Format( "Manager: Python entrypoint %s is not readable",
                                                    pluginFile.GetFullPath() ) );
            return;
        }

        break;
    }

    case PLUGIN_RUNTIME_TYPE::EXEC:
    {
        if( !pluginFile.IsFileExecutable() )
        {
            wxLogTrace( traceApi, wxString::Format( "Manager: Exec entrypoint %s is not executable",
                                                    pluginFile.GetFullPath() ) );
            return;
        }

        break;
    };

    default:
        wxLogTrace( traceApi, wxString::Format( "Manager: unhandled runtime for action %s",
                                                action->identifier ) );
        return;
    }

    args.emplace_back( pluginPath.wc_str() );

    for( const wxString& arg : action->args )
        args.emplace_back( arg.wc_str() );

    args.emplace_back( nullptr );

    wxExecuteEnv env;
    wxGetEnvMap( &env.env );
    env.env[ wxS( "KICAD_API_SOCKET" ) ] = Pgm().GetApiServer().SocketPath();
    env.env[ wxS( "KICAD_API_TOKEN" ) ] = Pgm().GetApiServer().Token();
    env.cwd = pluginFile.GetPath();

    long p = wxExecute( const_cast<wchar_t**>( args.data() ), wxEXEC_ASYNC, nullptr, &env );

    if( !p )
    {
        wxLogTrace( traceApi, wxString::Format( "Manager: launching action %s failed",
                                                action->identifier ) );
    }
    else
    {
        wxLogTrace( traceApi, wxString::Format( "Manager: launching action %s -> pid %ld",
                                                action->identifier, p ) );
    }
}


std::vector<const PLUGIN_ACTION*> API_PLUGIN_MANAGER::GetActionsForScope( PLUGIN_ACTION_SCOPE aScope )
{
    std::vector<const PLUGIN_ACTION*> actions;

    for( auto& [identifier, action] : m_actionsCache )
    {
        if( !m_readyPlugins.count( action->plugin.Identifier() ) )
            continue;

        if( action->scopes.count( aScope ) )
            actions.emplace_back( action );
    }

    return actions;
}


void API_PLUGIN_MANAGER::processPluginDependencies()
{
    for( const std::unique_ptr<API_PLUGIN>& plugin : m_plugins )
    {
        m_environmentCache[plugin->Identifier()] = wxEmptyString;

        if( plugin->Runtime().type != PLUGIN_RUNTIME_TYPE::PYTHON )
        {
            m_readyPlugins.insert( plugin->Identifier() );
            continue;
        }

        std::optional<wxString> env = PYTHON_MANAGER::GetPythonEnvironment( plugin->Identifier() );

        if( !env )
        {
            wxLogTrace( traceApi, wxString::Format( "Manager: could not create env for %s",
                                                     plugin->Identifier() ) );
            continue;
        }

        wxFileName envConfigPath( *env, wxS( "pyvenv.cfg" ) );

        if( envConfigPath.IsFileReadable() )
        {
            wxLogTrace( traceApi, wxString::Format( "Manager: Python env for %s exists at %s",
                                                    plugin->Identifier(), *env ) );
            JOB job;
            job.type = JOB_TYPE::INSTALL_REQUIREMENTS;
            job.identifier = plugin->Identifier();
            job.plugin_path = plugin->BasePath();
            job.env_path = *env;
            m_jobs.emplace_back( job );
            continue;
        }

        wxLogTrace( traceApi, wxString::Format( "Manager: will create Python env for %s at %s",
                                                plugin->Identifier(), *env ) );
        JOB job;
        job.type = JOB_TYPE::CREATE_ENV;
        job.identifier = plugin->Identifier();
        job.plugin_path = plugin->BasePath();
        job.env_path = *env;
        m_jobs.emplace_back( job );
    }

    wxCommandEvent evt;
    processNextJob( evt );
}


void API_PLUGIN_MANAGER::processNextJob( wxCommandEvent& aEvent )
{
    if( m_jobs.empty() )
    {
        wxLogTrace( traceApi, "Manager: cleared job queue" );
        return;
    }

    wxLogTrace( traceApi, wxString::Format( "Manager: begin processing; %zu jobs left in queue",
                                            m_jobs.size() ) );

    JOB& job = m_jobs.front();

    if( job.type == JOB_TYPE::CREATE_ENV )
    {
        wxLogTrace( traceApi, "Manager: Python exe '%s'",
                    Pgm().GetCommonSettings()->m_Api.python_interpreter );
        wxLogTrace( traceApi, wxString::Format( "Manager: creating Python env at %s",
                                                job.env_path ) );
        PYTHON_MANAGER manager( Pgm().GetCommonSettings()->m_Api.python_interpreter );

        manager.Execute( wxString::Format( wxS( "-m venv %s"), job.env_path ),
                [=]( int aRetVal, const wxString& aOutput, const wxString& aError )
                {
                    wxLogTrace( traceApi, wxString::Format( "Manager: venv (%d): %s",
                                                            aRetVal, aOutput ) );

                    if( !aError.IsEmpty() )
                        wxLogTrace( traceApi, wxString::Format( "Manager: venv err: %s", aError ) );

                    wxCommandEvent* evt = new wxCommandEvent( EDA_EVT_PLUGIN_MANAGER_JOB_FINISHED,
                                                              wxID_ANY );
                    QueueEvent( evt );
                } );
    }
    else if( job.type == JOB_TYPE::INSTALL_REQUIREMENTS )
    {
        wxLogTrace( traceApi, wxString::Format( "Manager: installing dependencies for %s",
                                                job.plugin_path ) );

        std::optional<wxString> pythonHome = PYTHON_MANAGER::GetPythonEnvironment( job.identifier );
        std::optional<wxString> python = PYTHON_MANAGER::GetVirtualPython( job.identifier );
        wxFileName reqs = wxFileName( job.plugin_path, "requirements.txt" );

        if( !python )
        {
            wxLogTrace( traceApi, wxString::Format( "Manager: error: python not found at %s",
                                                    job.env_path ) );
        }
        else if( !reqs.IsFileReadable() )
        {
            wxLogTrace( traceApi,
                        wxString::Format( "Manager: error: requirements.txt not found at %s",
                                          job.plugin_path ) );
        }
        else
        {
            wxLogTrace( traceApi, "Manager: Python exe '%s'", *python );

            PYTHON_MANAGER manager( *python );
            wxExecuteEnv env;

            if( pythonHome )
                env.env[wxS( "VIRTUAL_ENV" )] = *pythonHome;

            wxString cmd = wxS( "-m ensurepip" );
            wxLogTrace( traceApi, "Manager: calling python `%s`", cmd );

            manager.Execute( cmd,
                [=]( int aRetVal, const wxString& aOutput, const wxString& aError )
                {
                    wxLogTrace( traceApi, wxString::Format( "Manager: ensurepip (%d): %s",
                                                            aRetVal, aOutput ) );

                    if( !aError.IsEmpty() )
                    {
                        wxLogTrace( traceApi,
                                    wxString::Format( "Manager: ensurepip err: %s", aError ) );
                    }
                }, &env );

            cmd = wxString::Format(
                    wxS( "-m pip install --no-input --isolated --require-virtualenv "
                         "--exists-action i -r '%s'" ),
                    reqs.GetFullPath() );

            wxLogTrace( traceApi, "Manager: calling python `%s`", cmd );

            manager.Execute( cmd,
                [=]( int aRetVal, const wxString& aOutput, const wxString& aError )
                {
                    wxLogTrace( traceApi, wxString::Format( "Manager: pip (%d): %s",
                                                            aRetVal, aOutput ) );

                    if( !aError.IsEmpty() )
                        wxLogTrace( traceApi, wxString::Format( "Manager: pip err: %s", aError ) );

                    if( aRetVal == 0 )
                    {
                        wxLogTrace( traceApi, wxString::Format( "Manager: marking %s as ready",
                                                                job.identifier ) );
                        m_readyPlugins.insert( job.identifier );
                        wxCommandEvent* availabilityEvt =
                                new wxCommandEvent( EDA_EVT_PLUGIN_AVAILABILITY_CHANGED, wxID_ANY );
                        wxTheApp->QueueEvent( availabilityEvt );
                    }

                    wxCommandEvent* evt = new wxCommandEvent( EDA_EVT_PLUGIN_MANAGER_JOB_FINISHED,
                                                              wxID_ANY );

                    QueueEvent( evt );
                }, &env );
        }

        wxCommandEvent* evt = new wxCommandEvent( EDA_EVT_PLUGIN_MANAGER_JOB_FINISHED, wxID_ANY );
        QueueEvent( evt );
    }

    m_jobs.pop_front();
    wxLogTrace( traceApi, wxString::Format( "Manager: done processing; %zu jobs left in queue",
                                            m_jobs.size() ) );
}
