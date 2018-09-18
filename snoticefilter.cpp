/*
 * MIT License
 *
 * Copyright (C) 2018 Ryan Smith
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <regex.h>
#include <znc/Client.h>
#include <znc/Chan.h>
#include <znc/IRCNetwork.h>
#include <znc/Message.h>
#include <znc/Modules.h>
#include <znc/version.h>

#if (VERSION_MAJOR < 1) || (VERSION_MAJOR == 1 && VERSION_MINOR < 7)
#error The snoticefilter module requires ZNC version 1.7.0 or later.
#endif

class CSNoticeFilterMod : public CModule
{
    void OnAddFilterCommand(const CString &line)
    {
        const CString filter = line.Token(1, true);
        if(filter.empty())
        {
            PutModule("Usage: AddFilter <filter>");
            return;
        }

        if(ValidateRegPattern(filter))
        {
            m_vFilters.push_back(filter);
            PutModule("Filter added.");
            SaveFilters();
        }
        else
        {
            PutModule("Invalid filter pattern.");
        }
    }

    void OnDelFilterCommand(const CString &line)
    {
        u_int iNum = line.Token(1, true).ToUInt();

        if(iNum > m_vFilters.size() || iNum <= 0)
        {
            PutModule(t_s("Illegal # Requested"));
            return;
        }
        else
        {
            m_vFilters.erase(m_vFilters.begin() + iNum - 1);
            PutModule(t_s("Filter removed."));
        }
        SaveFilters();
    }

    void OnListFiltersCommand(const CString &line)
    {
        CTable Table;
        unsigned int index = 1;

        Table.AddColumn(t_s("Id", "list"));
        Table.AddColumn(t_s("Filter", "list"));

        for(const CString &sFilter : m_vFilters)
        {
            Table.AddRow();
            Table.SetCell(t_s("Id", "list"), CString(index++));
            Table.SetCell(t_s("Filter", "list"), "\"" + sFilter + "\"");
        }

        if(PutModule(Table) == 0)
        {
            PutModule(t_s("No filters saved."));
        } else {
            PutModule(t_s("Beginning and ending quotes around filters are added to show trailing spaces"));
        }
    }

    void OnAddClientCommand(const CString &line)
    {
        const CString identifier = line.Token(1);
        if(identifier.empty())
        {
            PutModule("Usage: AddClient <identifier>");
            return;
        }

        if(identifier == "*filters")
        {
            PutModule(t_s("You cannot access the *filters property directly"));
            return;
        }

        if(HasClient(identifier))
        {
            PutModule("Client already exists: " + identifier);
            return;
        }

        AddClient(identifier);
        PutModule("Client added: " + identifier);
    }

    void OnDelClientCommand(const CString &line)
    {
        const CString identifier = line.Token(1);
        if(identifier.empty())
        {
            PutModule("Usage: DelClient <identifier>");
            return;
        }

        if(identifier == "*filters")
        {
            PutModule(t_s("You cannot access the *filters property directly"));
            return;
        }

        if(!HasClient(identifier))
        {
            PutModule("Unknown client: " + identifier);
            return;
        }

        DelClient(identifier);
        PutModule("Client removed: " + identifier);
    }

    void OnListClientsCommand(const CString &line)
    {
        const CString current = GetClient()->GetIdentifier();

        CTable table;
        table.AddColumn(t_s("Client"));
        table.AddColumn(t_s("Connected"));
        table.AddColumn(t_s("Filter Enabled"));
        for(MCString::iterator it = BeginNV(); it != EndNV(); ++it)
        {
            if(it->first == "*filters")
            {
                continue;
            }
            table.AddRow();
            if(it->first == current)
            {
                table.SetCell(t_s("Client"), "*" + it->first);
            }
            else
            {
                table.SetCell(t_s("Client"), it->first);
            }
            table.SetCell(t_s("Connected"), CString(!GetNetwork()->FindClients(it->first).empty()));
            table.SetCell(t_s("Filter Enabled"), t_s(it->second == "true" ? "true" : "false"));
        }

        if(table.empty())
        {
            PutModule(t_s("No identified clients"));
        }
        else
        {
            PutModule(table);
        }
    }

    void OnToggleFilterCommand(const CString &line)
    {
        CString identifier = line.Token(1);
        if(identifier.empty())
        {
            identifier = GetClient()->GetIdentifier();
        }

        if(identifier.empty())
        {
            PutModule(t_s("Unidentified client"));
            return;
        }

        if(identifier == "*filters")
        {
            PutModule(t_s("You cannot access the *filters property directly"));
            return;
        }

        if(!HasClient(identifier))
        {
            PutModule(t_s("Unknown client: ") + identifier);
            return;
        }

        if(GetNV(identifier) == "true")
        {
            SetNV(identifier, "false");
            PutModule(t_s("Disabled filter for ") + identifier);
        }
        else
        {
            SetNV(identifier, "true");
            PutModule(t_s("Enabled filter for ") + identifier);
        }
    }

    bool IsClientFiltered(const CString &identifier)
    {
        if(identifier.empty())
        {
            return false;
        }

        if(!HasClient(identifier))
        {
            return false;
        }

        if(GetNV(identifier) == "true")
        {
            return true;
        }

        return false;
    }

    bool CheckRegString(CString sPattern, CString sMessage)
    {
        regex_t r;
        const char *pattern = sPattern.c_str();
        const char *message = sMessage.c_str();

        int status = regcomp(&r, pattern, REG_EXTENDED | REG_NOSUB);
        if(status != 0)
        {
            return false;
        }

        bool match = (regexec(&r, message, (size_t) 0, NULL, 0) == 0);
        regfree(&r);

        return match;
    }

    bool ValidateRegPattern(CString sPattern)
    {
        const char *pattern = sPattern.c_str();
        regex_t r;

        int status = regcomp(&r, pattern, REG_EXTENDED | REG_NOSUB);
        if(status != 0)
        {
            return false;
        }

        regfree(&r);

        return true;
    }

    bool AddClient(const CString &identifier)
    {
        return SetNV(identifier, "false");
    }

    bool DelClient(const CString &identifier)
    {
        return DelNV(identifier);
    }

    bool HasClient(const CString &identifier)
    {
        return !identifier.empty() && FindNV(identifier) != EndNV();
    }

public:
    bool OnLoad(const CString &sArgs, CString &sMessage)
    {
        GetNV("*filters").Split("\n", m_vFilters, false);

        return true;
    }

    CModule::EModRet OnSendToClientMessage(CMessage &sMessage)
    {
        CString nick = sMessage.GetNick().GetNick();
        if(nick.empty())
        {
            return CONTINUE;
        }

        CClient *client = CModule::GetClient();
        if(!client)
        {
            return CONTINUE;
        }

        const CString identifier = client->GetIdentifier();
        if(identifier.empty())
        {
            return CONTINUE;
        }

        // Assume a server notice if the nick contains a period
        if(sMessage.GetCommand() == "NOTICE" && sMessage.GetParams().size() >= 2 && IsClientFiltered(identifier) &&
           CheckRegString(CString("\\."), nick))
        {
            for(const CString &sFilter : m_vFilters)
            {
                if(CheckRegString(sFilter, sMessage.GetParam(1)))
                {
                    return HALTCORE;
                }
            }
        }

        return CONTINUE;
    }

    MODCONSTRUCTOR(CSNoticeFilterMod)
            {
                    AddHelpCommand();
                    AddCommand("AddClient", t_d("<identifier>"), t_d("Add a client."),[=](const CString &sLine) {
                OnAddClientCommand(sLine);
            });
                    AddCommand("DelClient", t_d("<identifier>"), t_d("Delete a client."),[=](const CString &sLine) {
                OnDelClientCommand(sLine);
            });
                    AddCommand("ListClients", "", t_d("List known clients and their filter status."),[=](const CString &sLine) {
                OnListClientsCommand(sLine);
            });
                    AddCommand("AddFilter", t_d("<filter>"), t_d("Add a filter."),[=](const CString &sLine) {
                OnAddFilterCommand(sLine);
            });
                    AddCommand("DelFilter", t_d("<number>"), t_d("Delete a filter."),[=](const CString &sLine) {
                OnDelFilterCommand(sLine);
            });
                    AddCommand("ListFilters", "", t_d("List filters."),[=](const CString &sLine) {
                OnListFiltersCommand(sLine);
            });
                    AddCommand("ToggleFilter", "", t_d("Toggle filter status for a client."),[=](const CString &sLine) {
                OnToggleFilterCommand(sLine);
            });
            }

private:
    VCString m_vFilters;

    void SaveFilters()
    {
        CString sBuffer = "";

        for(const CString &sFilter : m_vFilters)
        {
            sBuffer += sFilter + "\n";
        }

        SetNV("*filters", sBuffer);
    }
};

template<>
void TModInfo<CSNoticeFilterMod>(CModInfo &Info)
{
    Info.AddType(CModInfo::UserModule);
}

NETWORKMODULEDEFS(
        CSNoticeFilterMod,
        t_s("Filter server notices for identified clients")
)