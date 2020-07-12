#include "gli_file.h"

#include "log.h"
#include "zlib/zlib.h"
#include "zlib/contrib/minizip/unzip.h"

bool GliFileContainer::open(const char* path, GliFile*& handle)
{
    void* handlei = nullptr;

    if (!open_internal(path, handlei))
    {
        logf("GliFileContainer::open: open_internal failed for '%s'\n", path);
        return false;
    }

    handle = new GliFile(this, handlei);
    return true;
}


void GliFileContainer::close(GliFile* handle)
{
    if (handle)
    {
        close_internal(handle->_handle);
        delete handle;
    }
}


bool GliFileContainer::valid(GliFile* handle)
{
    return valid_internal(handle->_handle);
}


bool GliFileContainer::read_entire_file(const char* path, std::vector<uint8_t>& contents)
{
    return read_entire_file_internal(path, contents);
}


GliFile::GliFile(GliFileContainer* container, void* handle)
    : _container(container)
    , _handle()
{
}


GliFile::~GliFile()
{
    close();
}


bool GliFile::valid()
{
    return _container && _container->valid(this);
}


void GliFile::close()
{
    if (_container)
    {
        _container->close(this);
        _container = nullptr;
        _handle = nullptr;
    }
}


// TODO: File container representing the OS file system (or part thereof)
class GliFileContainerSystem : public GliFileContainer
{
public:
    bool attach(const char* container_name) override { return true; }
    void dettach() override {}
    bool open_internal(const char* path, void*& handle) override { return false; }
    void close_internal(void* handle) override {}
    bool valid_internal(void* handle) override { return false; }
    bool read_entire_file_internal(const char* path, std::vector<uint8_t>& contents) override { return false; }
};


class GliFileContainerZipFile : public GliFileContainer
{
public:
    bool attach(const char* container_name) override
    {
        _container_name = container_name;
        _unzfile = unzOpen(container_name);
        _file_opened = false;
        return !!_unzfile;
    }


    void dettach() override
    {
        if (_unzfile)
        {
            logm("Dettaching zip file container.\n");

            if (_file_opened)
            {
                unzCloseCurrentFile(_unzfile);
            }

            unzClose(_unzfile);
        }
    }


    bool open_internal(const char* path, void*& handle) override
    {
        if (!_unzfile)
        {
            logm("GliFileContainerZipFile::open_internal: Not attached.\n");
            return false;
        }

        if (_file_opened)
        {
            logm("GliFileContainerZipFile::open_internal: Only one file may be opened at a time.\n");
            return false;
        }

        if (unzLocateFile(_unzfile, path, 1))
        {
            logf("GliFileContainerZipFile::open_internal: File '%s' not found.\n", path);
            return false;
        }

        if (unzGetCurrentFileInfo(_unzfile, &_current_file_info, nullptr, 0, nullptr, 0, nullptr, 0))
        {
            logf("GliFileContainerZipFile::open_internal: Error getting file info for file '%s' [%d].\n", path, UNZ_ERRNO);
        }

        if (unzOpenCurrentFile(_unzfile))
        {
            logf("GliFileContainerZipFile::open_internal: Error opening '%s' [%d].\n", path, UNZ_ERRNO);
            return false;
        }

        _file_opened = true;

        return true;
    }


    void close_internal(void* handle) override
    {
        if (_unzfile && _file_opened)
        {
            unzCloseCurrentFile(_unzfile);
            _file_opened = false;
        }
    }


    bool valid_internal(void* handle) override { return _unzfile && _file_opened; }


    bool read_entire_file_internal(const char* path, std::vector<uint8_t>& contents) override
    {
        if (!_unzfile)
        {
            logm("GliFileContainerZipFile::read_entire_file_internal: Not attached.\n");
            return false;
        }

        if (_file_opened)
        {
            logm("GliFileContainerZipFile::read_entire_file_internal: Only one file may be opened at a time.\n");
            return false;
        }

        if (unzLocateFile(_unzfile, path, 1))
        {
            logf("GliFileContainerZipFile::read_entire_file_internal: File '%s' not found.\n", path);
            return false;
        }

        if (unzGetCurrentFileInfo(_unzfile, &_current_file_info, nullptr, 0, nullptr, 0, nullptr, 0))
        {
            logf("GliFileContainerZipFile::read_entire_file_internal: Error getting file info for file '%s' [%d].\n", path, UNZ_ERRNO);
        }

        if (unzOpenCurrentFile(_unzfile))
        {
            logf("GliFileContainerZipFile::read_entire_file_internal: Error opening '%s' [%d].\n", path, UNZ_ERRNO);
            return false;
        }

        contents.resize(_current_file_info.uncompressed_size);
        int read_result = unzReadCurrentFile(_unzfile, contents.data(), _current_file_info.uncompressed_size);

        unzCloseCurrentFile(_unzfile);

        // FIXME: unzReadCurrentFile's return result is ambiguous if the high bit of read size is set (unzReadCurrentFile returns either the number
        // of bytes read or a negative number if an error occurred, but the requested read size is unsigned).
        if ((uLong)read_result != _current_file_info.uncompressed_size)
        {
            logf("GliFileContainerZipFile::read_entire_file_internal: Error reading file '%s' [%d].\n", path, UNZ_ERRNO);
            return false;
        }

        return true;
    }

private:
    std::string _container_name;
    unzFile _unzfile = nullptr;
    bool _file_opened = false;
    unz_file_info _current_file_info;
};


GliFileSystem::~GliFileSystem()
{
    shutdown();
}


void GliFileSystem::shutdown()
{
    for (auto& container : _containers)
    {
        container->dettach();
    }
}


bool GliFileSystem::open(const char* path, GliFile*& handle)
{
    /*
        Paths are either raw system paths or paths to files in a container:
        //<container>//data/file.ext
    */
    bool success = false;
    std::string spath(path);
    GliFileContainer* container = nullptr;

    if (spath.rfind("//", 0) == 0)
    {
        size_t delim = spath.find("//", 2);

        if (delim == std::string::npos)
        {
            logf("GliFileSystem::open: Couldn't parse path '%s'.\n", path);
            return false;
        }

        std::string container_name = spath.substr(2, delim - 2);
        container = get_or_create_container(container_name);
        spath = spath.substr(delim + 2);
    }

    if (container)
    {
        success = container->open(spath.c_str(), handle);
    }
    else
    {
        logf("GliFileSystem::open: could not find container to open file '%s'.\n", path);
    }

    return success;
}


bool GliFileSystem::read_entire_file(const char* path, std::vector<uint8_t>& contents)
{
    bool success = false;
    std::string spath(path);
    GliFileContainer* container = nullptr;

    if (spath.rfind("//", 0) == 0)
    {
        size_t delim = spath.find("//", 2);

        if (delim == std::string::npos)
        {
            logf("GliFileSystem::read_entire_file: Couldn't parse path '%s'.\n", path);
            return false;
        }

        std::string container_name = spath.substr(2, delim - 2);
        container = get_or_create_container(container_name);
        spath = spath.substr(delim + 2);
    }

    if (container)
    {
        success = container->read_entire_file(spath.c_str(), contents);
    }
    else
    {
        logf("GliFileSystem::read_entire_file: could not find container to open file '%s'.\n", path);
    }

    return success;
}


GliFileContainer* GliFileSystem::get_or_create_container(const std::string& container_name)
{
    GliFileContainerLookup::const_iterator it = _container_lookup.find(container_name);

    if (it == _container_lookup.end())
    {
        GliFileContainerPtr new_container;
        size_t ext = container_name.rfind('.');

        if (ext != std::string::npos)
        {
            if (container_name.compare(ext, 3, ".zip"))
            {
                logf("GliFileSystem::get_or_create: Creating zip file container for '%s'\n", container_name.c_str());
                new_container = std::make_unique<GliFileContainerZipFile>();

                if (!new_container->attach(container_name.c_str()))
                {
                    logf("GliFileSystem::get_or_create: Failed to attach zip file container for '%s'\n", container_name.c_str());
                    return nullptr;
                }

                auto result = _container_lookup.emplace(container_name, _containers.size());

                if (!result.second)
                {
                    logf("GliFileSystem::get_or_create: Failed to insert container for '%s' into map\n", container_name.c_str());
                    return nullptr;
                }

                _containers.push_back(std::move(new_container));
                it = result.first;
            }
        }
    }

    return _containers[it->second].get();
}
