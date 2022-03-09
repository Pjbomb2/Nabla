#ifndef _NBL_SYSTEM_C_ARCHIVE_LOADER_ZIP_H_INCLUDED_
#define _NBL_SYSTEM_C_ARCHIVE_LOADER_ZIP_H_INCLUDED_


#include "nbl/system/CFileArchive.h"


namespace nbl::system
{

#if 0
		CFileArchiveZip(core::smart_refctd_ptr<system::IFile>&& file, core::smart_refctd_ptr<ISystem>&& system, bool isGZip, const std::string_view& password, system::logger_opt_smart_ptr&& logger = nullptr) :
			IFileArchive(std::move(file), std::move(system), std::move(logger)), m_isGZip(isGZip), m_password(password)
		{
			size_t offset = 0;
			if (m_file.get())
			{
				// load file entries
				if (m_isGZip)
					while (scanGZipHeader(offset)) {}
				else
					while (scanZipHeader(offset)) {}
			}
		}

		bool scanGZipHeader(size_t& offset);
		bool scanZipHeader(size_t& offset, bool ignoreGPBits = false);
		bool scanCentralDirectoryHeader(size_t& offset);
		E_ALLOCATOR_TYPE getAllocatorType(uint32_t compressionType)
		{
			switch (compressionType)
			{
			case 0: return EAT_NULL;
			default: return EAT_VIRTUAL_ALLOC;
			}
		}
#endif

class CArchiveLoaderZip final : public IArchiveLoader
{
	public:
		struct SZIPFileDataDescriptor
		{
			uint32_t CRC32;
			uint32_t CompressedSize;
			uint32_t UncompressedSize;
		};
		#include "nbl/nblpack.h"
		struct SZIPFileHeader
		{
			uint32_t Sig;				// 'PK0304' little endian (0x04034b50)
			int16_t VersionToExtract;
			int16_t GeneralBitFlag;
			int16_t CompressionMethod;
			int16_t LastModFileTime;
			int16_t LastModFileDate;
			SZIPFileDataDescriptor DataDescriptor;
			int16_t FilenameLength;
			int16_t ExtraFieldLength;
			// filename (variable size)
			// extra field (variable size )
		} PACK_STRUCT;
		#include "nbl/nblunpack.h"
		class CArchive final : public CFileArchive
		{
			public:
				CArchive(
					core::smart_refctd_ptr<IFile>&& _file,
					system::logger_opt_smart_ptr&& logger,
					core::vector<SListEntry>&& _items,
					core::vector<SZIPFileHeader>&& _itemsMetadata,
					const bool _isGZip
				) : CFileArchive(path(_file->getFileName()),std::move(logger),std::move(_items)),
					m_file(std::move(_file)), m_itemsMetadata(std::move(_itemsMetadata)),
					m_password(""), m_isGZip(_isGZip)
				{}

			private:
				file_buffer_t getFileBuffer(const IFileArchive::SListEntry* item) override;

				core::smart_refctd_ptr<IFile> m_file;
				core::vector<SZIPFileHeader> m_itemsMetadata;
				const std::string m_password; // TODO password
				const bool m_isGZip;
		};

		CArchiveLoaderZip(system::logger_opt_smart_ptr&& logger) : IArchiveLoader(std::move(logger)) {}

		inline bool isALoadableFileFormat(IFile* file) const override
		{
			SZIPFileHeader header;

			IFile::success_t succ;
			file->read(succ,&header,0,sizeof(header));

			return bool(succ) ||
				(header.Sig == 0x04034b50) || // ZIP
				(header.Sig & 0xffff) == 0x8b1f; // gzip
		}

		inline const char** getAssociatedFileExtensions() const override
		{
			static const char* ext[]{ "zip", "pk3", "tgz", "gz", nullptr};
			return ext;
		}

	private:
		core::smart_refctd_ptr<IFileArchive> createArchive_impl(core::smart_refctd_ptr<system::IFile>&& file, const std::string_view& password) const override;
};

}
#endif