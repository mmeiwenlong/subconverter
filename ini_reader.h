#ifndef INI_READER_H_INCLUDED
#define INI_READER_H_INCLUDED

#include <map>
#include <vector>
#include <algorithm>

typedef std::map<std::string, std::multimap<std::string, std::string>> ini_data_struct;
typedef std::multimap<std::string, std::string> string_multimap;

class INIReader
{
    /**
    *  @brief A simple INI reader which utilize map and vector
    *  to store sections and items, allowing access in logarithmic time.
    */
private:
    /**
    *  @brief Internal parsed flag.
    */
    bool parsed = false;
    std::string current_section;
    ini_data_struct ini_content;
    string_array exclude_sections, include_sections, read_sections;

    std::string cached_section;
    string_multimap cached_section_content;

    std::string isolated_items_section;

    bool chkIgnore(std::string section)
    {
        bool excluded = false, included = false;
        if(count(exclude_sections.begin(), exclude_sections.end(), section) > 0)
            excluded = true;
        if(include_sections.size() != 0)
        {
            if(count(include_sections.begin(), include_sections.end(), section) > 0)
                included = true;
        }
        else
            included = true;

        return excluded || !included;
    }
public:
    /**
    *  @brief Set this flag to true to do a UTF8-To-GBK conversion before parsing data. Only useful in Windows.
    */
    bool do_utf8_to_gbk = false;

    /**
    *  @brief Set this flag to true so any line within the section will be stored even it doesn't follow the "name=value" format.
    * These lines will store as the name "{NONAME}".
    */
    bool store_any_line = false;

    /**
    *  @brief Save isolated items before any section definitions.
    */
    bool store_isolated_line = false;

    /**
    *  @brief Allow a section title to appear multiple times.
    */
    bool allow_dup_section_titles = false;

    /**
    *  @brief Initialize the reader.
    */
    INIReader()
    {
        parsed = false;
    }

    /**
    *  @brief Parse a file during initialization.
    */
    INIReader(std::string filePath)
    {
        parsed = false;
        ParseFile(filePath);
    }

    ~INIReader()
    {
        //nothing to do
    }

    /**
    *  @brief Exclude a section with the given name.
    */
    void ExcludeSection(std::string section)
    {
        exclude_sections.emplace_back(section);
    }

    /**
    *  @brief Include a section with the given name.
    */
    void IncludeSection(std::string section)
    {
        include_sections.emplace_back(section);
    }

    /**
    *  @brief Set isolated items to given section.
    */
    void SetIsolatedItemsSection(std::string section)
    {
        isolated_items_section = section;
    }

    /**
    *  @brief Parse INI content into mapped data structure.
    * If exclude sections are set, these sections will not be stored.
    * If include sections are set, only these sections will be stored.
    */
    int Parse(std::string content) //parse content into mapped data
    {
        if(!content.size()) //empty content
            return -1;

        //remove UTF-8 BOM
        removeUTF8BOM(content);

        bool inExcludedSection = false;
        std::string strLine, thisSection, curSection, itemName, itemVal;
        string_multimap itemGroup, existItemGroup;
        std::stringstream strStrm;
        unsigned int lineSize = 0;
        char delimiter = count(content.begin(), content.end(), '\n') < 1 ? '\r' : '\n';

        EraseAll(); //first erase all data
        if(do_utf8_to_gbk && is_str_utf8(content))
            content = UTF8ToGBK(content); //do conversion if flag is set

        if(store_isolated_line)
            curSection = isolated_items_section; //items before any section define will be store in this section
        strStrm<<content;
        while(getline(strStrm, strLine, delimiter))
        //while(getline(strStrm, strLine)) //get one line of content
        {
            lineSize = strLine.size();
            if(!lineSize || strLine[0] == ';' || strLine[0] == '#') //empty lines and comments are ignored
                continue;
            if(strLine[lineSize - 1] == '\r') //remove line break
            {
                strLine = strLine.substr(0, lineSize - 1);
                lineSize--;
            }
            if(strLine.find("=") != strLine.npos) //is an item
            {
                if(inExcludedSection) //this section is excluded
                    continue;
                if(!curSection.size()) //not in any section
                    return -1;
                itemName = trim(strLine.substr(0, strLine.find("=")));
                itemVal = trim(strLine.substr(strLine.find("=") + 1));
                itemGroup.emplace(itemName, itemVal); //insert to current section
            }
            else if(strLine[0] == '[' && strLine[lineSize - 1] == ']') //is a section title
            {
                thisSection = strLine.substr(1, lineSize - 2); //save section title
                inExcludedSection = chkIgnore(thisSection); //check if this section is excluded

                if(curSection.size() && itemGroup.size()) //just finished reading a section
                {
                    if(ini_content.count(curSection)) //a section with the same name has been inserted
                    {
                        if(allow_dup_section_titles)
                        {
                            eraseElements(existItemGroup);
                            existItemGroup = ini_content.at(curSection); //get the old items
                            for(auto &x : existItemGroup)
                                itemGroup.emplace(x); //insert them all into new section
                            ini_content.erase(curSection); //remove the old section
                        }
                        else
                            return -1; //not allowed, stop
                    }
                    ini_content.emplace(curSection, itemGroup); //insert previous section to content map
                    read_sections.emplace_back(curSection); //add to read sections list
                }
                eraseElements(itemGroup); //reset section storage
                curSection = thisSection; //start a new section
            }
            else if(store_any_line && !inExcludedSection && curSection.size()) //store a line without name
            {
                itemGroup.emplace("{NONAME}", strLine);
            }
            if(include_sections.size() && include_sections == read_sections) //all included sections has been read
                break; //exit now
        }
        if(curSection.size() && itemGroup.size()) //final section
        {
            if(ini_content.count(curSection)) //a section with the same name has been inserted
            {
                if(allow_dup_section_titles)
                {
                    eraseElements(existItemGroup);
                    existItemGroup = ini_content.at(curSection); //get the old items
                    for(auto &x : existItemGroup)
                        itemGroup.emplace(x); //insert them all into new section
                    ini_content.erase(curSection); //remove the old section
                }
                else
                    return -1; //not allowed, stop
            }
            ini_content.emplace(curSection, itemGroup); //insert this section to content map
            read_sections.emplace_back(curSection); //add to read sections list
        }
        parsed = true;
        return 0; //all done
    }

    /**
    *  @brief Parse an INI file into mapped data structure.
    */
    int ParseFile(std::string filePath)
    {
        if(!fileExist(filePath))
            return -1;
        return Parse(fileGet(filePath, false));
    }

    /**
    *  @brief Check whether a section exist.
    */
    bool SectionExist(std::string section)
    {
        return ini_content.find(section) != ini_content.end();
    }

    /**
    *  @brief Count of sections in the whole INI.
    */
    unsigned int SectionCount()
    {
        return ini_content.size();
    }

    /**
    *  @brief Return all section names inside INI.
    */
    string_array GetSections()
    {
        string_array retData;

        for(auto &x : ini_content)
        {
            retData.emplace_back(x.first);
        }

        return retData;
    }

    /**
    *  @brief Enter a section with the given name. Section name and data will be cached to speed up the following reading process.
    */
    int EnterSection(std::string section)
    {
        if(!SectionExist(section))
            return -1;
        current_section = cached_section = section;
        cached_section_content = ini_content.at(section);
        return 0;
    }

    /**
    *  @brief Set current section.
    */
    void SetCurrentSection(std::string section)
    {
        current_section = section;
    }

    /**
    *  @brief Check whether an item exist in the given section. Return false if the section does not exist.
    */
    bool ItemExist(std::string section, std::string itemName)
    {
        if(!SectionExist(section))
            return false;

        if(section != cached_section)
        {
            cached_section = section;
            cached_section_content= ini_content.at(section);
        }

        return cached_section_content.find(itemName) != cached_section_content.end();
    }

    /**
    *  @brief Check whether an item exist in current section. Return false if the section does not exist.
    */
    bool ItemExist(std::string itemName)
    {
        return current_section.size() ? ItemExist(current_section, itemName) : false;
    }

    /**
    *  @brief Check whether an item with the given name prefix exist in the given section. Return false if the section does not exist.
    */
    bool ItemPrefixExist(std::string section, std::string itemName)
    {
        if(!SectionExist(section))
            return false;

        if(section != cached_section)
        {
            cached_section = section;
            cached_section_content= ini_content.at(section);
        }

        for(auto &x : cached_section_content)
        {
            if(x.first.find(itemName) == 0)
                return true;
        }

        return false;
    }

    /**
    *  @brief Check whether an item with the given name prefix exist in current section. Return false if the section does not exist.
    */
    bool ItemPrefixExist(std::string itemName)
    {
        return current_section.size() ? ItemPrefixExist(current_section, itemName) : false;
    }

    /**
    *  @brief Count of items in the given section. Return 0 if the section does not exist.
    */
    unsigned int ItemCount(std::string section)
    {
        if(!parsed || !SectionExist(section))
            return 0;

        return ini_content.at(section).size();
    }

    /**
    *  @brief Erase all data from the data structure and reset parser status.
    */
    void EraseAll()
    {
        eraseElements(ini_content);
        parsed = false;
    }

    /**
    *  @brief Retrieve all items in the given section.
    */
    int GetItems(std::string section, string_multimap &data)
    {
        if(!parsed || !SectionExist(section))
            return -1;

        if(cached_section != section)
        {
            cached_section = section;
            cached_section_content = ini_content.at(section);
        }

        data = cached_section_content;
        return 0;
    }

    /**
    *  @brief Retrieve all items in current section.
    */
    int GetItems(string_multimap &data)
    {
        return current_section.size() ? GetItems(current_section, data) : -1;
    }

    /**
    * @brief Retrieve item(s) with the same name prefix in the given section.
    */
    int GetAll(std::string section, std::string itemName, string_array &results) //retrieve item(s) with the same itemName prefix
    {
        if(!parsed)
            return -1;

        string_multimap mapTemp;

        if(GetItems(section, mapTemp) != 0)
            return -1;

        for(auto &x : mapTemp)
        {
            if(x.first.find(itemName) == 0)
                results.emplace_back(x.second);
        }

        return 0;
    }

    /**
    * @brief Retrieve item(s) with the same name prefix in current section.
    */
    int GetAll(std::string itemName, string_array &results)
    {
        return current_section.size() ? GetAll(current_section, itemName, results) : -1;
    }

    /**
    * @brief Retrieve one item with the exact same name in the given section.
    */
    std::string Get(std::string section, std::string itemName) //retrieve one item with the exact same itemName
    {
        if(!parsed)
            return std::string();

        string_multimap mapTemp;

        if(GetItems(section, mapTemp) != 0)
            return std::string();

        for(auto &x : mapTemp)
        {
            if(x.first == itemName)
                return x.second;
        }

        return std::string();
    }

    /**
    * @brief Retrieve one item with the exact same name in current section.
    */
    std::string Get(std::string itemName)
    {
        return current_section.size() ? Get(current_section, itemName) : std::string();
    }

    /**
    * @brief Retrieve one boolean item value with the exact same name in the given section.
    */
    bool GetBool(std::string section, std::string itemName)
    {
        return Get(section, itemName) == "true";
    }

    /**
    * @brief Retrieve one boolean item value with the exact same name in current section.
    */
    bool GetBool(std::string itemName)
    {
        return current_section.size() ? Get(current_section, itemName) == "true" : false;
    }

    /**
    * @brief Retrieve one integer item value with the exact same name in the given section.
    */
    int GetInt(std::string section, std::string itemName)
    {
        return stoi(Get(section, itemName));
    }

    /**
    * @brief Retrieve one integer item value with the exact same name in current section.
    */
    int GetInt(std::string itemName)
    {
        return GetInt(current_section, itemName);
    }

    /**
    * @brief Retrieve the first item found in the given section.
    */
    std::string GetFirst(std::string section, std::string itemName) //return the first item value found in section
    {
        if(!parsed)
            return std::string();
        string_array result;
        if(GetAll(section, itemName, result) != -1)
            return result[0];
        else
            return std::string();
    }

    /**
    * @brief Retrieve the first item found in current section.
    */
    std::string GetFirst(std::string itemName)
    {
        return current_section.size() ? GetFirst(current_section, itemName) : std::string();
    }

    /**
    * @brief Retrieve a string style array with specific separator and write into integer array.
    */
    template <typename T> void GetIntArray(std::string section, std::string itemName, std::string separator, T &Array)
    {
        string_array vArray;
        unsigned int index, UBound = sizeof(Array) / sizeof(Array[0]);
        vArray = split(Get(section, itemName), separator);
        for(index = 0; index < vArray.size() && index < UBound; index++)
            Array[index] = stoi(vArray[index]);
        for(; index < UBound; index++)
            Array[index] = 0;
    }

    /**
    * @brief Retrieve a string style array with specific separator and write into integer array.
    */
    template <typename T> void GetIntArray(std::string itemName, std::string separator, T &Array)
    {
        if(current_section.size())
            GetIntArray(current_section, itemName, separator, Array);
    }

    /**
    *  @brief Add a std::string value with given values.
    */
    int Set(std::string section, std::string itemName, std::string itemVal)
    {
        std::string value;

        if(!section.size())
            return -1;

        if(!parsed)
            parsed = true;

        if(SectionExist(section))
        {
            string_multimap &mapTemp = ini_content.at(section);
            mapTemp.insert(std::pair<std::string, std::string>(itemName, itemVal));
        }
        else
        {
            string_multimap mapTemp;
            mapTemp.insert(std::pair<std::string, std::string>(itemName, itemVal));
            ini_content.insert(std::pair<std::string, std::multimap<std::string, std::string>>(section, mapTemp));
        }

        return 0;
    }

    /**
    *  @brief Add a string value with given values.
    */
    int Set(std::string itemName, std::string itemVal)
    {
        if(!current_section.size())
            return -1;
        return Set(current_section, itemName, itemVal);
    }

    /**
    *  @brief Add a boolean value with given values.
    */
    int SetBool(std::string section, std::string itemName, bool itemVal)
    {
        return Set(section, itemName, itemVal ? "true" : "false");
    }

    /**
    *  @brief Add a boolean value with given values.
    */
    int SetBool(std::string itemName, bool itemVal)
    {
        return SetBool(current_section, itemName, itemVal);
    }

    /**
    *  @brief Add a double value with given values.
    */
    int SetDouble(std::string section, std::string itemName, double itemVal)
    {
        return Set(section, itemName, std::__cxx11::to_string(itemVal));
    }

    /**
    *  @brief Add a double value with given values.
    */
    int SetDouble(std::string itemName, double itemVal)
    {
        return SetDouble(current_section, itemName, itemVal);
    }

    /**
    *  @brief Add a long value with given values.
    */
    int SetLong(std::string section, std::string itemName, long itemVal)
    {
        return Set(section, itemName, std::__cxx11::to_string(itemVal));
    }

    /**
    *  @brief Add a long value with given values.
    */
    int SetLong(std::string itemName, long itemVal)
    {
        return SetLong(current_section, itemName, itemVal);
    }

    /**
    *  @brief Add an array with the given separator.
    */
    template <typename T> int SetArray(std::string section, std::string itemName, std::string separator, T &Array)
    {
        std::string data;
        for(auto &x : Array)
            data += std::__cxx11::to_string(x) + separator;
        data = data.substr(0, data.size() - separator.size());
        return Set(section, itemName, data);
    }

    /**
    *  @brief Add an array with the given separator.
    */
    template <typename T> int SetArray(std::string itemName, std::string separator, T &Array)
    {
        return current_section.size() ? SetArray(current_section, itemName, separator, Array) : -1;
    }

    /**
    *  @brief Erase all items with the given name.
    */
    int Erase(std::string section, std::string itemName)
    {
        int retVal;
        if(!SectionExist(section))
            return -1;

        retVal = ini_content.at(section).erase(itemName);
        if(retVal && cached_section == section)
        {
            cached_section_content = ini_content.at(section);
        }
        return retVal;
    }

    /**
    *  @brief Erase all items with the given name.
    */
    int Erase(std::string itemName)
    {
        return current_section.size() ? Erase(current_section, itemName) : -1;
    }

    /**
    *  @brief Erase the first item with the given name.
    */
    int EraseFirst(std::string section, std::string itemName)
    {
        string_multimap &mapTemp = ini_content.at(section);
        string_multimap::iterator iter = mapTemp.find(itemName);
        if(iter != mapTemp.end())
        {
            mapTemp.erase(iter);
            if(cached_section == section)
            {
                cached_section_content = mapTemp;
            }
            return 0;
        }
        else
        {
            return -1;
        }
    }

    /**
    *  @brief Erase the first item with the given name.
    */
    int EraseFirst(std::string itemName)
    {
        return current_section.size() ? EraseFirst(current_section, itemName) : -1;
    }

    /**
    *  @brief Erase all items in the given section.
    */
    void EraseSection(std::string section)
    {
        if(ini_content.find(section) == ini_content.end())
            return;
        eraseElements(ini_content.at(section));
        if(cached_section == section)
            eraseElements(cached_section_content);
    }

    /**
    *  @brief Erase all items in current section.
    */
    void EraseSection()
    {
        if(current_section.size())
            EraseSection(current_section);
    }

    /**
    *  @brief Export the whole INI data structure into a string.
    */
    std::string ToString()
    {
        std::string content;

        if(!parsed)
            return std::string();

        for(auto &x : ini_content)
        {
            content += "[" + x.first + "]\n";
            for(auto &y : x.second)
            {
                if(y.first != "{NONAME}")
                    content += y.first + " = ";
                content += y.second + "\n";
            }
            content += "\n";
        }

        return content.substr(0, content.size() - 2);
    }

    /**
    *  @brief Export the whole INI data structure into a file.
    */
    int ToFile(std::string filePath)
    {
        return fileWrite(filePath, ToString(), true);
    }
};

#endif // INI_READER_H_INCLUDED
