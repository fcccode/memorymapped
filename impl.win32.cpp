
#include <cassert>

namespace MemoryMapped
{
    size_t GetPageSize()
    {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        return sysInfo.dwAllocationGranularity;
    }


    bool File::open(const std::string& filename, size_t mappedBytes, CacheHint hint)
    {
        DWORD winHint;
        // already open ?
        if(isValid())
        {
            return false;
        }
        m_file       = 0;
        m_filesize   = 0;
        m_hint       = hint;
        m_mappedFile = NULL;
        m_mappedView = NULL;
        winHint = 0;
        switch (m_hint)
        {
            case Normal:
                winHint = FILE_ATTRIBUTE_NORMAL;
                break;
            case SequentialScan:
                winHint = FILE_FLAG_SEQUENTIAL_SCAN;
                break;
            case RandomAccess:
                winHint = FILE_FLAG_RANDOM_ACCESS;
                break;
            default:
                break;
        }
        // open file
        m_file = ::CreateFileA(
                filename.c_str(),
                GENERIC_READ, FILE_SHARE_READ, NULL,
                OPEN_EXISTING, winHint, NULL);
        if(!m_file)
        {
            return errOpenFail(filename, "CreateFileA() failed");
        }
        // file size
        LARGE_INTEGER result;
        if (!GetFileSizeEx(m_file, &result))
        {
            return errOpenFail(filename, "GetFileSizeEx() failed");
        }
        m_filesize = static_cast<uint64_t>(result.QuadPart);
        // convert to mapped mode
        m_mappedFile = ::CreateFileMapping(m_file, NULL, PAGE_READONLY, 0, 0, NULL);
        if (!m_mappedFile)
        {
            return errOpenFail(filename, "CreateFileMapping() failed");
        }
        // initial mapping
        remap(0, mappedBytes);
        if (!m_mappedView)
        {
            // remap() may throw an exception already
            return false;
        }
        // everything's fine
        return true;
    }

    void File::close()
    {
        m_filesize = 0;
        // kill pointer
        if (m_mappedView)
        {
            ::UnmapViewOfFile(m_mappedView);
            m_mappedView = NULL;
        }

        if(m_mappedFile)
        {
            ::CloseHandle(m_mappedFile);
            m_mappedFile = NULL;
        }
        //close underlying file
        if(m_file)
        {
            ::CloseHandle(m_file);
            m_file = 0;
        }
    }

    bool File::remap(uint64_t offset, size_t mappedBytes)
    {
        DWORD offsetLow;
        DWORD offsetHigh;
        if (!m_file)
        {
            throw IOError("trying to operate on a closed handle");
            return false;
        }
        if (mappedBytes == WholeFile)
        {
            mappedBytes = m_filesize;
        }
        // close old mapping
        if(m_mappedView)
        {
            ::UnmapViewOfFile(m_mappedView);
            m_mappedView = NULL;
        }
        // don't go further than end of file
        if(offset > m_filesize)
        {
            return errOffset();
        }
        if((offset + mappedBytes) > m_filesize)
        {
            mappedBytes = size_t(m_filesize - offset);
        }
        offsetLow  = DWORD(offset & 0xFFFFFFFF);
        offsetHigh = DWORD(offset >> 32);
        m_mappedBytes = mappedBytes;
        // get memory address
        m_mappedView = ::MapViewOfFile(m_mappedFile, FILE_MAP_READ, offsetHigh, offsetLow, mappedBytes);
        if(m_mappedView == NULL)
        {
            m_mappedBytes = 0;
            m_mappedView  = NULL;
            throw IOError("MapViewOfFile() == NULL");
            return false;
        }
        return true;
    }
}
