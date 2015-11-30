#include <memory>
#include "Acquisition.h"
#include "DownloadRequest.h"
#include "LcpUtils.h"
#include "Public/ILicense.h"
#include "ICryptoProvider.h"

namespace lcp
{
    /*static*/ const char * Acquisition::PublicationType = u8"application/epub+zip";
    /*static*/ const float Acquisition::DownloadCoefficient = static_cast<float>(0.9);

    Acquisition::Acquisition(
        ILicense * license,
        IFileSystemProvider * fileSystemProvider,
        INetProvider * netProvider,
        ICryptoProvider * cryptoProvider,
        const std::string & publicationPath
        )
        : m_license(license)
        , m_fileSystemProvider(fileSystemProvider)
        , m_netProvider(netProvider)
        , m_cryptoProvider(cryptoProvider)
        , m_callback(nullptr)
        , m_publicationPath(publicationPath)
    {
    }

    Status Acquisition::Start(IAcquisitionCallback * callback)
    {
        try
        {
            m_callback = callback;

            ILinks * links = m_license->Links();
            if (!links->Has(Publication))
            {
                return Status(StCodeCover::ErrorAcquisitionNoAcquisitionLink);
            }
            Link publicationLink;
            links->GetLink(Publication, publicationLink);
            if (publicationLink.type != PublicationType)
            {
                return Status(StCodeCover::ErrorAcquisitionPublicationWrongType);
            }

            m_file.reset(m_fileSystemProvider->GetFile(m_publicationPath));
            if (m_file.get() == nullptr)
            {
                return Status(StCodeCover::ErrorAcquisitionInvalidFilePath);
            }

            m_request.reset(new DownloadRequest(publicationLink.href, m_file.get()));
            m_netProvider->StartDownloadRequest(m_request.get(), this);

            return this->CheckPublicationHash(publicationLink);
        }
        catch (const StatusException & ex)
        {
            return ex.ResultStatus();
        }
        catch (const std::exception & ex)
        {
            return Status(StCodeCover::ErrorCommonFail, ex.what());
        }
    }

    Status Acquisition::CheckPublicationHash(const Link & link)
    {
        if (!link.hash.empty())
        {
            std::vector<unsigned char> rawHash;
            Status res = m_cryptoProvider->CalculateFileHash(m_file.get(), rawHash);
            if (!Status::IsSuccess(res))
                return res;

            std::string hexHash;
            res = m_cryptoProvider->ConvertRawToHex(rawHash, hexHash);
            if (!Status::IsSuccess(res))
                return res;

            if (hexHash != link.hash)
            {
                return Status(StCodeCover::ErrorAcquisitionPublicationCorrupted);
            }
        }
        return Status(StCodeCover::ErrorCommonSuccess);
    }

    void Acquisition::Cancel()
    {
        m_request->SetCanceled(true);
    }

    std::string Acquisition::PublicationPath() const
    {
        return m_file->GetPath();
    }

    std::string Acquisition::SuggestedFileName() const
    {
        return m_request->SuggestedFileName();
    }

    void Acquisition::OnRequestStarted(INetRequest * request)
    {
        if (m_callback != nullptr)
        {
            m_callback->OnAcquisitionStarted(this);
        }
    }
    void Acquisition::OnRequestProgressed(INetRequest * request, float progress)
    {
        if (m_callback != nullptr)
        {
            m_callback->OnAcquisitionProgressed(this, progress * DownloadCoefficient);
        }
    }
    void Acquisition::OnRequestCanceled(INetRequest * request)
    {
        if (m_callback != nullptr)
        {
            m_callback->OnAcquisitionCanceled(this);
        }
    }
    void Acquisition::OnRequestEnded(INetRequest * request, Status result)
    {
        if (m_callback != nullptr)
        {
            m_callback->OnAcquisitionEnded(this, result);
        }
    }
}