// Copyright (C) 2012, Chris J. Foster and the other authors and contributors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of the software's owners nor the names of its
//   contributors may be used to endorse or promote products derived from this
//   software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// (This is the BSD 3-clause license)

#ifndef STREAM_PAGE_CACHE_H_INCLUDED
#define STREAM_PAGE_CACHE_H_INCLUDED

#include <algorithm>
#include <assert.h>
#include <fstream>
#include <memory>
#include <string.h>
#include <stdexcept>
#include <stdint.h>
#include <unordered_map>
#include <unordered_set>

/// Application controlled page cache for access to raw file data
///
/// This interface allows the application to specify data to be fetched, along
/// with a priority for the data.
class StreamPageCache
{
    public:
        typedef uint64_t PosType;

        StreamPageCache(std::istream& input, PosType pageSize = 512*1024)
            : m_input(input),
            m_pageSize(pageSize)
        {
            m_input.seekg(0, std::ios::end);
            m_fileSize = static_cast<PosType>(m_input.tellg());
            m_input.seekg(0);
            if (!m_input)
                throw std::runtime_error("Page cache could not open file");
        }

        /// Mark pages overlapping the given range for fetching
        ///
        /// Page priority is taken as the maximum of any fetch requests which
        /// overlap the given page.
        ///
        /// prefetch() does not do any actual fetching of data;  it returns
        /// immediately with status indicating whether the data is already
        /// present in the cache.
        bool prefetch(PosType offset, PosType length, double priority = 0)
        {
            if (offset + length > m_fileSize)
                throw std::runtime_error("Prefetch request past end of file");
            PosType pagesBegin = pageIndex(offset);
            PosType pagesEnd = pageIndex(offset + length - 1) + 1;
            bool inCache = true;
            for (PosType pageIdx = pagesBegin; pageIdx < pagesEnd; ++pageIdx)
            {
                auto page = m_pages.find(pageIdx);
                if (page == m_pages.end())
                {
                    auto pendingPage = m_pendingPages.find(pageIdx);
                    if (pendingPage == m_pendingPages.end())
                        m_pendingPages[pageIdx] = priority;
                    else if (pendingPage->second < priority)
                        pendingPage->second = priority;
                    inCache = false;
                }
            }
            return inCache;
        }

        /// Attempt to read length bytes into buf, starting at offset
        ///
        /// If the byte range is not in the cache, return false (the user may
        /// call prefetch() to bring these into cache)
        bool read(char* buf, PosType offset, PosType length)
        {
            PosType pagesBegin = pageIndex(offset);
            PosType pagesEnd = pageIndex(offset + length - 1) + 1;
            //tfm::printf("read(): pagesBegin = %d, pagesEnd = %d\n", pagesBegin, pagesEnd);
            for (PosType pageIdx = pagesBegin; pageIdx < pagesEnd; ++pageIdx)
            {
                auto page = m_pages.find(pageIdx);
                if (page == m_pages.end())
                {
                    //tfm::printf("Didn't find page %d\n", pageIdx);
                    return false;
                }
                PosType pageOffsetBegin = pageIdx * m_pageSize;
                PosType pageOffsetEnd   = (pageIdx+1) * m_pageSize;
                // Range of bytes to copy within page
                PosType byteBegin = (pageOffsetBegin < offset) ?
                                    offset - pageOffsetBegin : 0;
                PosType byteEnd   = (pageOffsetEnd > offset + length) ?
                                    offset + length - pageOffsetBegin : m_pageSize;
                PosType nbytes = byteEnd - byteBegin;
                //tfm::printf("read(): byteBegin = %d, byteEnd = %d\n", byteBegin, byteEnd);
                memcpy(buf, page->second.get() + byteBegin, nbytes);
                buf += nbytes;
            }
            return true;
        }

        /// Fetch a bunch of pages which have been previously marked.
        ///
        /// Eventually this will probably happen in the background
        size_t fetchNow(size_t numFetch)
        {
            typedef std::pair<double, PosType> PendingPage;
            std::vector<PendingPage> priorityPages;
            for (auto p = m_pendingPages.begin(); p != m_pendingPages.end(); ++p)
                priorityPages.push_back(PendingPage(p->second, p->first));
            numFetch = std::min(numFetch, priorityPages.size());
            std::nth_element(priorityPages.begin(), priorityPages.begin() + numFetch,
                             priorityPages.end(), std::greater<PendingPage>());
            for (size_t i = 0; i < numFetch; ++i)
            {
                PosType pageIdx = priorityPages[i].second;
                std::unique_ptr<char[]>& buf = m_pages[pageIdx];
                assert(!buf);
                buf.reset(new char[m_pageSize]);
                PosType pageOffset = pageIdx*m_pageSize;
                m_input.seekg(pageOffset);
                m_input.read(buf.get(), std::min(m_pageSize, m_fileSize - pageOffset));
                m_pendingPages.erase(pageIdx);
            }
            return numFetch;
        }

    private:
        PosType pageIndex(PosType address) const
        {
            return address/m_pageSize;
        }

        std::istream& m_input;
        PosType m_pageSize;
        PosType m_fileSize;
        std::unordered_map<PosType, double> m_pendingPages;
        std::unordered_map<PosType, std::unique_ptr<char[]>> m_pages;
};


#endif // STREAM_PAGE_CACHE_H_INCLUDED
