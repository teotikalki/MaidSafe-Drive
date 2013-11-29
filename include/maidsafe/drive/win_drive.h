/*  Copyright 2011 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.maidsafe.net/licenses

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */

#ifndef MAIDSAFE_DRIVE_WIN_DRIVE_H_
#define MAIDSAFE_DRIVE_WIN_DRIVE_H_

#include <Windows.h>

#include <chrono>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#pragma pack(push, r1, 8)
#include "CbFs.h"  // NOLINT
#pragma pack(pop, r1)

#include "boost/filesystem/path.hpp"
#include "boost/preprocessor/stringize.hpp"
#include "boost/thread/future.hpp"

#include "maidsafe/common/utils.h"
#include "maidsafe/encrypt/self_encryptor.h"

#include "maidsafe/drive/drive.h"
#include "maidsafe/drive/directory_handler.h"
#include "maidsafe/drive/utils.h"

namespace maidsafe {

namespace drive {

template <typename Storage>
class CbfsDrive;

namespace detail {

struct DirectoryEnumerationContext {
  explicit DirectoryEnumerationContext(const Directory& directory_in)
      : exact_match(false), directory(directory_in) {}
  DirectoryEnumerationContext() : exact_match(false), directory() {}
  bool exact_match;
  detail::Directory directory;
};

template <typename Storage>
CbfsDrive<Storage>* GetDrive(CallbackFileSystem* sender) {
  return static_cast<CbfsDrive<Storage>*>(sender->GetTag());
}

template <typename Storage>
boost::filesystem::path GetRelativePath(CbfsDrive<Storage>* cbfs_drive, CbFsFileInfo* file_info) {
  assert(file_info);
  std::unique_ptr<WCHAR[]> file_name(new WCHAR[cbfs_drive->max_file_path_length()]);
  file_info->get_FileName(file_name.get());
  return boost::filesystem::path(file_name.get());
}

void ErrorMessage(const std::string& method_name, ECBFSError error);

}  // namespace detail

template <typename Storage>
class CbfsDrive : public Drive<Storage> {
 public:
  CbfsDrive(std::shared_ptr<Storage> storage, const Identity& unique_user_id,
            const Identity& root_parent_id, const boost::filesystem::path& mount_dir,
            const boost::filesystem::path& user_app_dir, const boost::filesystem::path& drive_name,
            bool create);

  virtual ~CbfsDrive();

  void Mount();
  bool Unmount();
  int Install();
  uint32_t max_file_path_length();

 private:
  CbfsDrive(const CbfsDrive&);
  CbfsDrive(CbfsDrive&&);
  CbfsDrive& operator=(CbfsDrive);

  void UnmountDrive(const std::chrono::steady_clock::duration& timeout_before_force);
  std::wstring drive_name() const;

  void FlushAll();
  void UpdateDriverStatus();
  void UpdateMountingPoints();
  void OnCallbackFsInit();
  int OnCallbackFsInstall();
  void OnCallbackFsUninstall();
  void OnCallbackFsDeleteStorage();
  void OnCallbackFsMount();
  void OnCallbackFsUnmount();
  void OnCallbackFsAddPoint(const boost::filesystem::path&);
  void OnCallbackFsDeletePoint();
  virtual void SetNewAttributes(FileContextPtr file_context, bool is_directory, bool read_only);

  static void CbFsMount(CallbackFileSystem* sender);
  static void CbFsUnmount(CallbackFileSystem* sender);
  static void CbFsGetVolumeSize(CallbackFileSystem* sender, __int64* total_number_of_sectors,
                                __int64* number_of_free_sectors);
  static void CbFsGetVolumeLabel(CallbackFileSystem* sender, LPTSTR volume_label);
  static void CbFsSetVolumeLabel(CallbackFileSystem* sender, LPCTSTR volume_label);
  static void CbFsGetVolumeId(CallbackFileSystem* sender, PDWORD volume_id);
  static void CbFsCreateFile(CallbackFileSystem* sender, LPCTSTR file_name,
                             ACCESS_MASK desired_access, DWORD file_attributes, DWORD share_mode,
                             CbFsFileInfo* file_info, CbFsHandleInfo* handle_info);
  static void CbFsOpenFile(CallbackFileSystem* sender, LPCWSTR file_name,
                           ACCESS_MASK desired_access, DWORD file_attributes, DWORD share_mode,
                           CbFsFileInfo* file_info, CbFsHandleInfo* handle_info);
  static void CbFsCloseFile(CallbackFileSystem* sender, CbFsFileInfo* file_info,
                            CbFsHandleInfo* handle_info);
  static void CbFsGetFileInfo(CallbackFileSystem* sender, LPCTSTR file_name, LPBOOL file_exists,
                              PFILETIME creation_time, PFILETIME last_access_time,
                              PFILETIME last_write_time, __int64* end_of_file,
                              __int64* allocation_size, __int64* file_id, PDWORD file_attributes,
                              LPWSTR short_file_name OPTIONAL,
                              PWORD short_file_name_length OPTIONAL,
                              LPWSTR real_file_name OPTIONAL,
                              LPWORD real_file_name_length OPTIONAL);
  static void CbFsEnumerateDirectory(
      CallbackFileSystem* sender, CbFsFileInfo* directory_info, CbFsHandleInfo* handle_info,
      CbFsDirectoryEnumerationInfo* directory_enumeration_info, LPCWSTR mask, INT index,
      BOOL restart, LPBOOL file_found, LPWSTR file_name, PDWORD file_name_length,
      LPWSTR short_file_name OPTIONAL, PUCHAR short_file_name_length OPTIONAL,
      PFILETIME creation_time, PFILETIME last_access_time, PFILETIME last_write_time,
      __int64* end_of_file, __int64* allocation_size, __int64* file_id, PDWORD file_attributes);
  static void CbFsCloseDirectoryEnumeration(CallbackFileSystem* sender,
                                            CbFsFileInfo* directory_info,
                                            CbFsDirectoryEnumerationInfo* enumeration_info);
  static void CbFsSetAllocationSize(CallbackFileSystem* sender, CbFsFileInfo* file_info,
                                    __int64 allocation_size);
  static void CbFsSetEndOfFile(CallbackFileSystem* sender, CbFsFileInfo* file_info,
                               __int64 end_of_file);
  static void CbFsSetFileAttributes(CallbackFileSystem* sender, CbFsFileInfo* file_info,
                                    CbFsHandleInfo* handle_info, PFILETIME creation_time,
                                    PFILETIME last_access_time, PFILETIME last_write_time,
                                    DWORD file_attributes);
  static void CbFsCanFileBeDeleted(CallbackFileSystem* sender, CbFsFileInfo* file_info,
                                   CbFsHandleInfo* handle_info, LPBOOL can_be_deleted);
  static void CbFsDeleteFile(CallbackFileSystem* sender, CbFsFileInfo* file_info);
  static void CbFsRenameOrMoveFile(CallbackFileSystem* sender, CbFsFileInfo* file_info,
                                   LPCTSTR new_file_name);
  static void CbFsReadFile(CallbackFileSystem* sender, CbFsFileInfo* file_info, __int64 position,
                           PVOID buffer, DWORD bytes_to_read, PDWORD bytes_read);
  static void CbFsWriteFile(CallbackFileSystem* sender, CbFsFileInfo* file_info, __int64 position,
                            PVOID buffer, DWORD bytes_to_write, PDWORD bytes_written);
  static void CbFsIsDirectoryEmpty(CallbackFileSystem* sender, CbFsFileInfo* directory_info,
                                   LPWSTR file_name, LPBOOL is_empty);
  static void CbFsSetFileSecurity(CallbackFileSystem* sender, CbFsFileInfo* file_info,
                                  PVOID file_handle_context,
                                  SECURITY_INFORMATION security_information,
                                  PSECURITY_DESCRIPTOR security_descriptor, DWORD length);
  static void CbFsGetFileSecurity(CallbackFileSystem* sender, CbFsFileInfo* file_info,
                                  PVOID file_handle_context,
                                  SECURITY_INFORMATION security_information,
                                  PSECURITY_DESCRIPTOR security_descriptor, DWORD length,
                                  PDWORD length_needed);
  static void CbFsFlushFile(CallbackFileSystem* sender, CbFsFileInfo* file_info);
  static void CbFsOnEjectStorage(CallbackFileSystem* sender);

  mutable CallbackFileSystem callback_filesystem_;
  std::string guid_;
  LPCWSTR icon_id_;
  std::wstring drive_name_;
  LPCSTR registration_key_;
  boost::promise<void> unmounted_;
  std::once_flag unmounted_once_flag_;
};

template <typename Storage>
CbfsDrive<Storage>::CbfsDrive(std::shared_ptr<Storage> storage, const Identity& unique_user_id,
                              const Identity& root_parent_id,
                              const boost::filesystem::path& mount_dir,
                              const boost::filesystem::path& user_app_dir,
                              const boost::filesystem::path& drive_name, bool create)
    : Drive(storage, unique_user_id, root_parent_id, mount_dir, user_app_dir, create),
      callback_filesystem_(),
      guid_(BOOST_PP_STRINGIZE(CBFS_GUID)),
      icon_id_(L"MaidSafeDriveIcon"),
      drive_name_(drive_name.wstring()),
      registration_key_(BOOST_PP_STRINGIZE(CBFS_KEY)),
      unmounted_(),
      unmounted_once_flag_() {}

template <typename Storage>
CbfsDrive<Storage>::~CbfsDrive() {
  Unmount();
}

template <typename Storage>
void CbfsDrive<Storage>::Mount() {
#ifndef NDEBUG
    int timeout_milliseconds(0);
#else
    int timeout_milliseconds(30000);
#endif
  try {
    OnCallbackFsInit();
    UpdateDriverStatus();
    callback_filesystem_.Initialize(guid_.data());
    callback_filesystem_.CreateStorage();
    // SetIcon can only be called after CreateStorage has successfully completed.
    callback_filesystem_.SetIcon(icon_id_);
    callback_filesystem_.SetTag(static_cast<void*>(this));
    callback_filesystem_.MountMedia(timeout_milliseconds);
    // The following can only be called when the media is mounted.
    callback_filesystem_.AddMountingPoint(kMountDir_.c_str());
    UpdateMountingPoints();
  }
  catch (const ECBFSError& error) {
    detail::ErrorMessage("Mount: ", error);
    ThrowError(CommonErrors::uninitialised);
  }
  catch (const std::exception& e) {
    LOG(kError) << "Mount: " << e.what();
    ThrowError(CommonErrors::uninitialised);
  }

  LOG(kInfo) << "Mounted.";
  auto wait_until_unmounted(unmounted_.get_future());
  wait_until_unmounted.get();
}

template <typename Storage>
void CbfsDrive<Storage>::UnmountDrive(
    const std::chrono::steady_clock::duration& timeout_before_force) {
  std::chrono::steady_clock::time_point timeout(std::chrono::steady_clock::now() +
                                                timeout_before_force);
  while (callback_filesystem_.Active()) {
    try {
      for (int index = callback_filesystem_.GetMountingPointCount() - 1; index >= 0; --index)
        callback_filesystem_.DeleteMountingPoint(index);
      callback_filesystem_.UnmountMedia(std::chrono::steady_clock::now() < timeout);
    }
    catch (const ECBFSError&) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
}

template <typename Storage>
bool CbfsDrive<Storage>::Unmount() {
  try {
    std::call_once(unmounted_once_flag_, [&]() {
        // Only one instance of this lambda function can be run simultaneously.  If any CBFS
        // function throws, the unmounted_once_flag_ remains unset and another attempt can be made.
        UnmountDrive(std::chrono::seconds(3));
        if (callback_filesystem_.StoragePresent())
          callback_filesystem_.DeleteStorage();
        callback_filesystem_.SetRegistrationKey(nullptr);
        unmounted_.set_value();
    });
  }
  catch (const ECBFSError& error) {
    detail::ErrorMessage("Unmount", error);
    return false;
  }
  return true;
}

template <typename Storage>
uint32_t CbfsDrive<Storage>::max_file_path_length() {
  return static_cast<uint32_t>(callback_filesystem_.GetMaxFilePathLength());
}

template <typename Storage>
std::wstring CbfsDrive<Storage>::drive_name() const {
  return drive_name_;
}

template <typename Storage>
void CbfsDrive<Storage>::FlushAll() {
  directory_handler_.FlushAll();
}

template <typename Storage>
void CbfsDrive<Storage>::UpdateDriverStatus() {
  BOOL installed = false;
  INT version_high = 0, version_low = 0;
  SERVICE_STATUS status;
  CallbackFileSystem::GetModuleStatus(guid_.data(), CBFS_MODULE_DRIVER, &installed, &version_high,
                                      &version_low, &status);
  if (installed) {
    LPTSTR string_status = L"in undefined state";
    switch (status.dwCurrentState) {
      case SERVICE_CONTINUE_PENDING:
        string_status = L"continue is pending";
        break;
      case SERVICE_PAUSE_PENDING:
        string_status = L"pause is pending";
        break;
      case SERVICE_PAUSED:
        string_status = L"is paused";
        break;
      case SERVICE_RUNNING:
        string_status = L"is running";
        break;
      case SERVICE_START_PENDING:
        string_status = L"is starting";
        break;
      case SERVICE_STOP_PENDING:
        string_status = L"is stopping";
        break;
      case SERVICE_STOPPED:
        string_status = L"is stopped";
        break;
      default:
        string_status = L"in undefined state";
    }
    LOG(kInfo) << "Driver (version " << (version_high >> 16) << "." << (version_high & 0xFFFF)
               << "." << (version_low >> 16) << "." << (version_low & 0xFFFF)
               << ") installed, service " << string_status;
  }
}

template <typename Storage>
void CbfsDrive<Storage>::UpdateMountingPoints() {
  DWORD flags;
  LUID authentication_id;
  LPTSTR mounting_point = nullptr;
  for (int index = callback_filesystem_.GetMountingPointCount() - 1; index >= 0; --index) {
    callback_filesystem_.GetMountingPoint(index, &mounting_point, &flags, &authentication_id);
    if (mounting_point) {
      free(mounting_point);
      mounting_point = nullptr;
    }
  }
}

template <typename Storage>
void CbfsDrive<Storage>::OnCallbackFsInit() {
  try {
    callback_filesystem_.SetRegistrationKey(registration_key_);
    callback_filesystem_.SetOnStorageEjected(CbFsOnEjectStorage);
    callback_filesystem_.SetOnMount(CbFsMount);
    callback_filesystem_.SetOnUnmount(CbFsUnmount);
    callback_filesystem_.SetOnGetVolumeSize(CbFsGetVolumeSize);
    callback_filesystem_.SetOnGetVolumeLabel(CbFsGetVolumeLabel);
    callback_filesystem_.SetOnSetVolumeLabel(CbFsSetVolumeLabel);
    callback_filesystem_.SetOnGetVolumeId(CbFsGetVolumeId);
    callback_filesystem_.SetOnCreateFile(CbFsCreateFile);
    callback_filesystem_.SetOnOpenFile(CbFsOpenFile);
    callback_filesystem_.SetOnCloseFile(CbFsCloseFile);
    callback_filesystem_.SetOnGetFileInfo(CbFsGetFileInfo);
    callback_filesystem_.SetOnEnumerateDirectory(CbFsEnumerateDirectory);
    callback_filesystem_.SetOnCloseDirectoryEnumeration(CbFsCloseDirectoryEnumeration);
    callback_filesystem_.SetOnSetAllocationSize(CbFsSetAllocationSize);
    callback_filesystem_.SetOnSetEndOfFile(CbFsSetEndOfFile);
    callback_filesystem_.SetOnSetFileAttributes(CbFsSetFileAttributes);
    callback_filesystem_.SetOnCanFileBeDeleted(CbFsCanFileBeDeleted);
    callback_filesystem_.SetOnDeleteFile(CbFsDeleteFile);
    callback_filesystem_.SetOnRenameOrMoveFile(CbFsRenameOrMoveFile);
    callback_filesystem_.SetOnReadFile(CbFsReadFile);
    callback_filesystem_.SetOnWriteFile(CbFsWriteFile);
    callback_filesystem_.SetOnIsDirectoryEmpty(CbFsIsDirectoryEmpty);
    callback_filesystem_.SetOnFlushFile(CbFsFlushFile);
    callback_filesystem_.SetSerializeCallbacks(true);
    callback_filesystem_.SetFileCacheEnabled(false);
    callback_filesystem_.SetMetaDataCacheEnabled(false);
//    callback_filesystem_.SetStorageCharacteristics(
//        CallbackFileSystem::CbFsStorageCharacteristics(
//            CallbackFileSystem::scRemovableMedia |
//            CallbackFileSystem::scShowInEjectionTray |
//            CallbackFileSystem::scAllowEjection));
//    callback_filesystem_.SetStorageType(CallbackFileSystem::stDiskPnP);
  }
  catch (const ECBFSError& error) {
    detail::ErrorMessage("OnCallbackFsInit", error);
  }
  return;
}

template <typename Storage>
int CbfsDrive<Storage>::Install() {
  return OnCallbackFsInstall();
}

template <typename Storage>
int CbfsDrive<Storage>::OnCallbackFsInstall() {
  TCHAR file_name[MAX_PATH];
  DWORD reboot = 0;

  if (!GetModuleFileName(nullptr, file_name, MAX_PATH)) {
    DWORD error = GetLastError();
    detail::ErrorMessage("OnCallbackFsInstall::GetModuleFileName", error);
    return error;
  }
  try {
    boost::filesystem::path drive_path(
        boost::filesystem::path(file_name).parent_path().parent_path());
    boost::filesystem::path cab_path(drive_path / "drivers\\cbfs\\cbfs.cab");
    LOG(kInfo) << "CbfsDrive::OnCallbackFsInstall cabinet file: " << cab_path.string();

    callback_filesystem_.Install(
      cab_path.wstring().c_str(), guid_.data(), boost::filesystem::path().wstring().c_str(), false,
      CBFS_MODULE_DRIVER | CBFS_MODULE_NET_REDIRECTOR_DLL | CBFS_MODULE_MOUNT_NOTIFIER_DLL,
      &reboot);
    return reboot;
  }
  catch (const ECBFSError& error) {
    detail::ErrorMessage("OnCallbackFsInstall", error);
    return -1111;
  }
}

// =============================== Callbacks =======================================================

// Quote from CBFS documentation:
//
// This event is fired after Callback File System mounts the storage and makes it available. The
// event is optional - you don't have to handle it.
template <typename Storage>
void CbfsDrive<Storage>::CbFsMount(CallbackFileSystem* /*sender*/) {
  LOG(kInfo) << "CbFsMount";
}

// Quote from CBFS documentation:
//
// This event is fired after Callback File System unmounts the storage and it becomes unavailable
// for the system. The event is optional - you don't have to handle it.
template <typename Storage>
void CbfsDrive<Storage>::CbFsUnmount(CallbackFileSystem* /*sender*/) {
  LOG(kInfo) << "CbFsUnmount";
}

// Quote from CBFS documentation:
//
// This event is fired when the OS wants to obtain information about the size and available space on
// the disk. Minimal size of the volume accepted by Windows is 6144 bytes (based on 3072-byte sector
// and 2 sectors per cluster), however CBFS adjusts the size to be at least 16 sectors to ensure
// compatibility with possible changes in future versions of Windows.
template <typename Storage>
void CbfsDrive<Storage>::CbFsGetVolumeSize(CallbackFileSystem* sender,
                                           int64_t* total_number_of_sectors,
                                           int64_t* number_of_free_sectors) {
  LOG(kInfo) << "CbFsGetVolumeSize";
  WORD sector_size(sender->GetSectorSize());
  *total_number_of_sectors = (std::numeric_limits<int64_t>::max() - 10000) / sector_size;
  *number_of_free_sectors = (std::numeric_limits<int64_t>::max() - 10000) / sector_size;
}

// Quote from CBFS documentation:
//
// This event is fired when the OS wants to obtain the volume label.
template <typename Storage>
void CbfsDrive<Storage>::CbFsGetVolumeLabel(CallbackFileSystem* sender, LPTSTR volume_label) {
  LOG(kInfo) << "CbFsGetVolumeLabel";
  auto cbfs_drive(GetDrive(sender));
  wcsncpy_s(volume_label, cbfs_drive->drive_name().size() + 1, &cbfs_drive->drive_name()[0],
            cbfs_drive->drive_name().size() + 1);
}

// Quote from CBFS documentation:
//
// This event is fired when the OS wants to change the volume label.
template <typename Storage>
void CbfsDrive<Storage>::CbFsSetVolumeLabel(CallbackFileSystem* /*sender*/,
                                            LPCTSTR /*volume_label*/) {
  LOG(kInfo) << "CbFsSetVolumeLabel";
}

// Quote from CBFS documentation:
//
// This event is fired when Callback File System wants to obtain the volume Id. The volume Id is
// unique user defined value (within Callback File System volumes).
template <typename Storage>
void CbfsDrive<Storage>::CbFsGetVolumeId(CallbackFileSystem* /*sender*/, PDWORD volume_id) {
  LOG(kInfo) << "CbFsGetVolumeId";
  *volume_id = 0x68451321;
}

// Quote from CBFS documentation:
//
// This event is fired when the OS wants to create a file or directory with given name and
// attributes. The directories are created with this call.
//
// To check, what should be created (file or directory), check FileAttributes as follows
// (C++ / C# notation):
// Directory = FileAttributes & FILE_ATTRIBUTE_DIRECTORY == FILE_ATTRIBUTE_DIRECTORY;
//
// If the file name contains semicolon (":"), this means that the request is made to create a named
// stream in a file. The part before the semicolon is the name of the file itself and the name after
// the semicolon is the name of the named stream. If you don't want to deal with named streams,
// don't implement the handler for OnEnumerateNamedStreams event. In this case CBFS API will tell
// the OS that the named streams are not supported by the file system.
//
// DesiredAccess, ShareMode and Attributes are passed as they were specified in the call to
// CreateFile() Windows API function.
//
// The application can use FileInfo's and HandleInfo's UserContext property to store the reference
// to some information, identifying the file or directory (such as file/directory handle or database
// record ID or reference to the stream class etc). The value, set in the event handler, is later
// passed to all operations, related to this file, together with file/directory name and attributes.
//
// Note, that if CallAllOpenCloseCallbacks property is set to false (default value), then this event
// is fired only when the first handle to the file is opened.
//
// Sometimes it can happen that OnCreateFile is fired for a file which already exists. Normally such
// situation will not happen, as the OS knows which files exist before creating or opening files
// (this information is requested via OnGetFileInfo and OnEnumerateDirectory). However, if your
// files come from outside, a race condition can happen and the file will exist externally but will
// not be known to the OS and to CBFS yet. In this case you need to decide based on your application
// logic - you can either truncate an existing file or report the error. ERROR_ALREADY_EXISTS is a
// proper error code in this situation.
template <typename Storage>
void CbfsDrive<Storage>::CbFsCreateFile(CallbackFileSystem* sender, LPCTSTR file_name,
                                        ACCESS_MASK /*desired_access*/,
                                        DWORD file_attributes, DWORD /*share_mode*/,
                                        CbFsFileInfo* file_info,
                                        CbFsHandleInfo* /*handle_info*/) {
  SCOPED_PROFILE
  boost::filesystem::path relative_path(file_name);
  LOG(kInfo) << "CbFsCreateFile - " << relative_path << " 0x" << std::hex << file_attributes;

  bool is_directory((file_attributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY);
  try {
    detail::FileContext file_context(relative_path.filename(), is_directory);
    file_context->meta_data.attributes = file_attributes;
    GetDrive(sender)->Create(relative_path, std::move(file_context));
  }
  catch (const drive_error& error) {
    LOG(kWarning) << "CbfsCreateFile: " << relative_path << ": " << error.what();
    if (error.code() == make_error_code(DriveErrors::file_exists))
      throw ECBFSError(ERROR_ALREADY_EXISTS);
    else
      throw ECBFSError(ERROR_ACCESS_DENIED);
  }
  catch (const std::exception& e) {
    LOG(kError) << "CbfsCreateFile: " << relative_path << ": " << e.what();
    throw ECBFSError(ERROR_ACCESS_DENIED);
  }
}

// Quote from CBFS documentation:
//
// This event is fired when the OS wants to open an existing file or directory with given name and
// attributes. The directory can be opened, for example, in order to change it's attributes or to
// enumerate it's contents.
//
// If the file name contains semicolon (":"), this means that the request is made to open a named
// stream in a file. The part before the semicolon is the name of the file itself and the name after
// the semicolon is the name of the named stream. If you don't want to deal with named streams,
// don't implement the handler for OnEnumerateNamedStreams event. In this case CBFS API will tell
// the OS that the named streams are not supported by the file system.
//
// The application can use FileInfo's and HandleInfo's UserContext property to store the reference
// to some information, identifying the file or directory (such as file/directory handle or database
// record ID or reference to the stream class etc). The value, set in the event handler, is later
// passed to all operations, related to this file, together with file/directory name and attributes.
//
// Note, that if CallAllOpenCloseCallbacks property is set to false (default value), then this event
// is fired only when the first handle to the file is opened.
//
// Sometimes it can happen that OnOpenFile is fired for a file which doesn't already exist. Normally
// such situation will not happen, as the OS knows which files exist before creating or opening
// files (this information is requested via OnGetFileInfo and OnEnumerateDirectory). However, if
// your files are created and deleted from outside, a race condition can happen and the file will
// disappear but will still be known to the OS and to CBFS. In this case you need to report
// ERROR_FILE_NOT_FOUND error.
template <typename Storage>
void CbfsDrive<Storage>::CbFsOpenFile(CallbackFileSystem* sender, LPCWSTR file_name,
                                      ACCESS_MASK /*desired_access*/,
                                      DWORD /*file_attributes*/, DWORD /*share_mode*/,
                                      CbFsFileInfo* /*file_info*/,
                                      CbFsHandleInfo* /*handle_info*/) {
  SCOPED_PROFILE
  LOG(kInfo) << "CbFsOpenFile - " << boost::filesystem::path(file_name);
  try {
    GetDrive(sender)->Open(file_name);
  }
  catch (const drive_error& error) {
    LOG(kWarning) << "CbFsOpenFile: " << boost::filesystem::path(file_name) << ": " << error.what();
    if (error.code() == make_error_code(DriveErrors::no_such_file))
      throw ECBFSError(ERROR_FILE_NOT_FOUND);
    else
      throw ECBFSError(ERROR_ERRORS_ENCOUNTERED);
  }
  catch (const std::exception& e) {
    LOG(kError) << "CbFsOpenFile: " << boost::filesystem::path(file_name) << ": " << e.what();
    throw ECBFSError(ERROR_ERRORS_ENCOUNTERED);
  }
}

// Quote from CBFS documentation:
//
// This event is fired when the OS needs to close the previously created or opened file. Use
// FileInfo and HandleInfo to identify the file that needs to be closed.
//
// Note, that if CallAllOpenCloseCallbacks property is set to false (default value), then this event
// is fired only after the last handle to the file is closed.
template <typename Storage>
void CbfsDrive<Storage>::CbFsCloseFile(CallbackFileSystem* sender, CbFsFileInfo* file_info,
                                       CbFsHandleInfo* /*handle_info*/) {
  SCOPED_PROFILE
  auto cbfs_drive(GetDrive(sender));
  auto relative_path(detail::GetRelativePath<Storage>(cbfs_drive, file_info));
  LOG(kInfo) << "CbFsCloseFile - " << relative_path;
  try {
    cbfs_drive->Release(file_name);
  }
  catch (const drive_error& error) {
    LOG(kError) << "CbFsCloseFile: " << relative_path << ": " << error.what();
    if (error.code() == make_error_code(DriveErrors::no_such_file))
      throw ECBFSError(ERROR_FILE_NOT_FOUND);
    else
      throw ECBFSError(ERROR_ERRORS_ENCOUNTERED);
  }
  catch (const std::exception& e) {
    LOG(kError) << "CbFsCloseFile: " << relative_path << ": " << e.what();
    throw ECBFSError(ERROR_ERRORS_ENCOUNTERED);
  }
}

// Quote from CBFS documentation:
//
// The event is fired when the OS needs to get information about the file or directory. If the file
// exists, FileExists parameter must be set to true and all information (besides optional
// parameters) must be set. If the file doesn't exist, then FileExists must be set to false. In this
// case no parameters are read back.
//
// If you have enabled short file name support, your callback should return short name (which must
// not exceed 12 characters in 8.3 format) via ShortFileName parameter. Note, that it is possible
// that the OS sends you short filename in FileName parameter, in which case you still need to
// provide the same short name in ShortFileName parameter.
//
// If you have enabled case-sensitive file name support, the driver might need to ask your code for
// "normalized" filename. This means that if the driver gets a request for "QWERTY.txt", but only
// "qwErTy.TxT" file exists on the filesystem, your code can return the existing file name using
// RealFileName parameter.
//
// To speed-up operations (save one string length measurement per name) the driver doesn't measure
// the length of the passed short and real file names, so your code must put the length of the
// passed file names into ShortFileNameLength and RealFileNameLength parameters.
template <typename Storage>
void CbfsDrive<Storage>::CbFsGetFileInfo(
    CallbackFileSystem* sender, LPCTSTR file_name, LPBOOL file_exists, PFILETIME creation_time,
    PFILETIME last_access_time, PFILETIME last_write_time, int64_t* end_of_file,
    int64_t* allocation_size, int64_t* file_id OPTIONAL, PDWORD file_attributes,
    LPWSTR /*short_file_name*/ OPTIONAL, PWORD /*short_file_name_length*/ OPTIONAL,
    LPWSTR real_file_name OPTIONAL, LPWORD real_file_name_length OPTIONAL) {
  SCOPED_PROFILE
  boost::filesystem::path relative_path(file_name);
  LOG(kInfo) << "CbFsGetFileInfo - " << relative_path;
  detail::FileContext* file_context(nullptr);
  try {
    auto cbfs_drive(GetDrive(sender));
    file_context = cbfs_drive->GetContext(relative_path);
  }
  catch (const std::exception& e) {
    *file_exists = false;
    *file_attributes = 0xFFFFFFFF;
    throw ECBFSError(ERROR_FILE_NOT_FOUND);
  }

  *file_exists = true;
  *creation_time = file_context->meta_data.creation_time;
  *last_access_time = file_context->meta_data.last_access_time;
  *last_write_time = file_context->meta_data.last_write_time;
  //if (file_context->meta_data.end_of_file < file_context->meta_data.allocation_size)
  //  file_context->meta_data.end_of_file = file_context->meta_data.allocation_size;
  //else if (file_context->meta_data.allocation_size < file_context->meta_data.end_of_file)
  //  file_context->meta_data.allocation_size = file_context->meta_data.end_of_file;
  *end_of_file = file_context->meta_data.end_of_file;
  *allocation_size = file_context->meta_data.allocation_size;
  //*file_id = 0;
  *file_attributes = file_context->meta_data.attributes;
  *real_file_name = file_context->meta_data.name.wstring();
  *real_file_name_length = file_context->meta_data.name.wstring().size();
}

template <typename Storage>
void CbfsDrive<Storage>::CbFsEnumerateDirectory(
    CallbackFileSystem* sender, CbFsFileInfo* directory_info, CbFsHandleInfo* /*handle_info*/,
    CbFsDirectoryEnumerationInfo* directory_enumeration_info, LPCWSTR mask, INT index,
    BOOL restart, LPBOOL file_found, LPWSTR file_name, PDWORD file_name_length,
    LPWSTR /*short_file_name*/ OPTIONAL, PUCHAR /*short_file_name_length*/ OPTIONAL,
    PFILETIME creation_time, PFILETIME last_access_time, PFILETIME last_write_time,
    __int64* end_of_file, __int64* allocation_size, __int64* file_id, PDWORD file_attributes) {
  SCOPED_PROFILE
  auto cbfs_drive(GetDrive(sender));
  auto relative_path(detail::GetRelativePath<Storage>(cbfs_drive, directory_info));
  detail::DirectoryEnumerationContext* enum_context = nullptr;
  std::wstring mask_str(mask);
  LOG(kInfo) << "CbFsEnumerateDirectory - " << relative_path << " index: " << index
             << std::boolalpha << " nullptr context: " << (directory_enumeration_info == nullptr)
             << " mask: " << WstringToString(mask_str) << " restart: " << restart;
  bool exact_match(mask_str != L"*");
  *file_found = false;

  if (restart && directory_enumeration_info->get_UserContext()) {
    enum_context = static_cast<detail::DirectoryEnumerationContext*>(
        directory_enumeration_info->get_UserContext());
    delete enum_context;
    enum_context = nullptr;
    directory_enumeration_info->set_UserContext(nullptr);
  }

  Directory directory;
  if (!directory_enumeration_info->get_UserContext()) {
    try {
      directory = cbfs_drive->GetDirectory(relative_path);
    }
    catch (...) {
      throw ECBFSError(ERROR_FILE_NOT_FOUND);
    }
    enum_context = new detail::DirectoryEnumerationContext(directory);
    enum_context->directory.listing->ResetChildrenIterator();
    directory_enumeration_info->set_UserContext(enum_context);
  } else {
    enum_context = static_cast<detail::DirectoryEnumerationContext*>(
        directory_enumeration_info->get_UserContext());
    if (restart)
      enum_context->directory.listing->ResetChildrenIterator();
  }

  MetaData meta_data;
  if (exact_match) {
    while (!(*file_found)) {
      if (!enum_context->directory.listing->GetChildAndIncrementItr(meta_data))
        break;
      *file_found = detail::MatchesMask(mask_str, meta_data.name);
    }
  } else {
    *file_found = enum_context->directory.listing->GetChildAndIncrementItr(meta_data);
  }

  if (*file_found) {
    // Need to use wcscpy rather than the secure wcsncpy_s as file_name has a size of 0 in some
    // cases.  CBFS docs specify that callers must assign MAX_PATH chars to file_name, so we assume
    // this is done.
    wcscpy(file_name, meta_data.name.wstring().c_str());
    *file_name_length = static_cast<DWORD>(meta_data.name.wstring().size());
    *creation_time = meta_data.creation_time;
    *last_access_time = meta_data.last_access_time;
    *last_write_time = meta_data.last_write_time;
    *end_of_file = meta_data.end_of_file;
    *allocation_size = meta_data.allocation_size;
    *file_id = 0;
    *file_attributes = meta_data.attributes;
  }
  enum_context->exact_match = exact_match;
}

template <typename Storage>
void CbfsDrive<Storage>::CbFsCloseDirectoryEnumeration(
    CallbackFileSystem* sender, CbFsFileInfo* directory_info,
    CbFsDirectoryEnumerationInfo* directory_enumeration_info) {
  SCOPED_PROFILE
  auto cbfs_drive(GetDrive(sender));
  auto relative_path(detail::GetRelativePath<Storage>(cbfs_drive, directory_info));
  LOG(kInfo) << "CbFsCloseEnumeration - " << relative_path;
  if (directory_enumeration_info) {
    auto enum_context = static_cast<detail::DirectoryEnumerationContext*>(
        directory_enumeration_info->get_UserContext());
    delete enum_context;
    enum_context = nullptr;
    directory_enumeration_info->set_UserContext(nullptr);
  }
}

template <typename Storage>
void CbfsDrive<Storage>::CbFsSetAllocationSize(CallbackFileSystem* sender, CbFsFileInfo* file_info,
                                               int64_t allocation_size) {
  SCOPED_PROFILE
  auto cbfs_drive(GetDrive(sender));
  auto relative_path(detail::GetRelativePath<Storage>(cbfs_drive, file_info));
  LOG(kInfo) << "CbFsSetAllocationSize - " << relative_path << " to " << allocation_size
             << " bytes.";
  if (!file_info->get_UserContext())
    return;

  FileContextPtr file_context(static_cast<FileContextPtr>(file_info->get_UserContext()));
  if (file_context->meta_data.allocation_size == file_context->meta_data.end_of_file)
    return;

  if (cbfs_drive->TruncateFile(file_context, allocation_size)) {
    file_context->meta_data.allocation_size = allocation_size;
    if (!file_context->self_encryptor->Flush()) {
      LOG(kError) << "CbFsSetAllocationSize: " << relative_path << ", failed to flush";
    }
  }
  file_context->content_changed = true;
}

template <typename Storage>
void CbfsDrive<Storage>::CbFsSetEndOfFile(CallbackFileSystem* sender, CbFsFileInfo* file_info,
                                          int64_t end_of_file) {
  SCOPED_PROFILE
  auto cbfs_drive(GetDrive(sender));
  auto relative_path(detail::GetRelativePath<Storage>(cbfs_drive, file_info));
  LOG(kInfo) << "CbFsSetEndOfFile - " << relative_path << " to " << end_of_file << " bytes.";
  if (!file_info->get_UserContext())
    return;

  FileContextPtr file_context(static_cast<FileContextPtr>(file_info->get_UserContext()));
  if (cbfs_drive->TruncateFile(file_context, end_of_file)) {
    file_context->meta_data.end_of_file = end_of_file;
    if (!file_context->self_encryptor->Flush()) {
      LOG(kError) << "CbFsSetEndOfFile: " << relative_path << ", failed to flush";
    }
  } else {
    LOG(kError) << "Truncate failed for " << file_context->meta_data.name;
  }

  if (file_context->meta_data.allocation_size == static_cast<uint64_t>(end_of_file))
    return;

  file_context->meta_data.allocation_size = end_of_file;
  file_context->content_changed = true;
}

template <typename Storage>
void CbfsDrive<Storage>::CbFsSetFileAttributes(
    CallbackFileSystem* sender, CbFsFileInfo* file_info, CbFsHandleInfo* /*handle_info*/,
    PFILETIME creation_time, PFILETIME last_access_time, PFILETIME last_write_time,
    DWORD file_attributes) {
  SCOPED_PROFILE
  auto cbfs_drive(GetDrive(sender));
  auto relative_path(detail::GetRelativePath<Storage>(cbfs_drive, file_info));
  LOG(kInfo) << "CbFsSetFileAttributes- " << relative_path << " 0x" << std::hex << file_attributes;
  if (!file_info->get_UserContext())
    return;

  FileContextPtr file_context(static_cast<FileContextPtr>(file_info->get_UserContext()));
  if (file_attributes != 0)
    file_context->meta_data.attributes = file_attributes;
  if (creation_time)
    file_context->meta_data.creation_time = *creation_time;
  if (last_access_time)
    file_context->meta_data.last_access_time = *last_access_time;
  if (last_write_time)
    file_context->meta_data.last_write_time = *last_write_time;

  file_context->content_changed = true;
}

template <typename Storage>
void CbfsDrive<Storage>::CbFsCanFileBeDeleted(CallbackFileSystem* /*sender*/,
                                              CbFsFileInfo* /*file_info*/,
                                              CbFsHandleInfo* /*handle_info*/,
                                              LPBOOL can_be_deleted) {
  SCOPED_PROFILE
  // auto cbfs_drive(GetDrive(sender));
  // auto relative_path(detail::GetRelativePath<Storage>(cbfs_drive, file_info));
  LOG(kInfo) << "CbFsCanFileBeDeleted - ";  //  << relative_path;
  *can_be_deleted = true;
}

template <typename Storage>
void CbfsDrive<Storage>::CbFsDeleteFile(CallbackFileSystem* sender, CbFsFileInfo* file_info) {
  SCOPED_PROFILE
  auto cbfs_drive(GetDrive(sender));
  auto relative_path(detail::GetRelativePath<Storage>(cbfs_drive, file_info));
  LOG(kInfo) << "CbFsDeleteFile - " << relative_path;
  try {
    cbfs_drive->Delete(relative_path);
  }
  catch (...) {
    throw ECBFSError(ERROR_FILE_NOT_FOUND);
  }
}

template <typename Storage>
void CbfsDrive<Storage>::CbFsRenameOrMoveFile(CallbackFileSystem* sender, CbFsFileInfo* file_info,
                                              LPCTSTR new_file_name) {
  SCOPED_PROFILE
  auto cbfs_drive(GetDrive(sender));
  auto old_relative_path(detail::GetRelativePath<Storage>(cbfs_drive, file_info));
  boost::filesystem::path new_relative_path(new_file_name);
  LOG(kInfo) << "CbFsRenameOrMoveFile - " << old_relative_path << " to " << new_relative_path;
  FileContext file_context;
  try {
    file_context = cbfs_drive->GetFileContext(old_relative_path);
  }
  catch (...) {
    throw ECBFSError(ERROR_FILE_NOT_FOUND);
  }
  try {
    cbfs_drive->Rename(old_relative_path, new_relative_path, *file_context.meta_data);
  }
  catch (...) {
    throw ECBFSError(ERROR_ACCESS_DENIED);
  }
}

template <typename Storage>
void CbfsDrive<Storage>::CbFsReadFile(CallbackFileSystem* sender, CbFsFileInfo* file_info,
                                      int64_t position, PVOID buffer, DWORD bytes_to_read,
                                      PDWORD bytes_read) {
  SCOPED_PROFILE
  auto cbfs_drive(GetDrive(sender));
  auto relative_path(detail::GetRelativePath<Storage>(cbfs_drive, file_info));
  if (!file_info->get_UserContext())
    return;

  FileContextPtr file_context(static_cast<FileContextPtr>(file_info->get_UserContext()));
  LOG(kInfo) << "CbFsReadFile- " << relative_path << " reading " << bytes_to_read << " of "
              << file_context->meta_data.end_of_file << " at position " << position;
  assert(file_context->self_encryptor);
  *bytes_read = 0;

  if (!file_context->self_encryptor)
    throw ECBFSError(ERROR_INVALID_PARAMETER);
  if (!file_context->self_encryptor->Read(static_cast<char*>(buffer), bytes_to_read, position))
    throw ECBFSError(ERROR_FILE_NOT_FOUND);

  if (static_cast<uint64_t>(position + bytes_to_read) > file_context->self_encryptor->size())
    *bytes_read = static_cast<DWORD>(file_context->self_encryptor->size() - position);
  else
    *bytes_read = bytes_to_read;
  GetSystemTimeAsFileTime(&file_context->meta_data.last_access_time);
  file_context->content_changed = true;
}

template <typename Storage>
void CbfsDrive<Storage>::CbFsWriteFile(CallbackFileSystem* sender, CbFsFileInfo* file_info,
                                       int64_t position, PVOID buffer, DWORD bytes_to_write,
                                       PDWORD bytes_written) {
  SCOPED_PROFILE
  auto cbfs_drive(GetDrive(sender));
  auto relative_path(detail::GetRelativePath<Storage>(cbfs_drive, file_info));
  LOG(kInfo) << "CbFsWriteFile- " << relative_path << " writing " << bytes_to_write
             << " bytes at position " << position;

  FileContext* file_context(static_cast<FileContext*>(file_info->get_UserContext()));
  assert(file_context);
  assert(file_context->self_encryptor);
  *bytes_written = 0;
  if (!file_context->self_encryptor->Write(static_cast<char*>(buffer), bytes_to_write, position))
    throw ECBFSError(ERROR_FILE_NOT_FOUND);

  *bytes_written = bytes_to_write;
  GetSystemTimeAsFileTime(&file_context->meta_data.last_write_time);
  file_context->content_changed = true;
}

template <typename Storage>
void CbfsDrive<Storage>::CbFsIsDirectoryEmpty(CallbackFileSystem* sender,
                                              CbFsFileInfo* /*directory_info*/, LPWSTR file_name,
                                              LPBOOL is_empty) {
  SCOPED_PROFILE
  LOG(kInfo) << "CbFsIsDirectoryEmpty - " << boost::filesystem::path(file_name);
  try {
    auto cbfs_drive(GetDrive(sender));
    *is_empty = cbfs_drive->GetDirectory(boost::filesystem::path(file_name)).listing->empty();
  }
  catch (...) {
    throw ECBFSError(ERROR_FILE_NOT_FOUND);
  }
}

// This event is fired when the OS tells the file system, that file buffers (incuding all possible
// metadata) must be flushed and written to the backend storage. FileInfo contains information about
// the file to be flushed. If FileInfo is empty, your code should attempt to flush everything
// related to the disk.
template <typename Storage>
void CbfsDrive<Storage>::CbFsFlushFile(CallbackFileSystem* sender, CbFsFileInfo* file_info) {
  SCOPED_PROFILE
  auto cbfs_drive(GetDrive(sender));
  if (!file_info) {
    try {
      cbfs_drive->FlushAll();
    }
    catch (const std::exception& e) {
      LOG(kError) << "CbFsFlushFile for all files: " << e.what();
      throw ECBFSError(ERROR_ERRORS_ENCOUNTERED);
    }
  }

  auto relative_path(detail::GetRelativePath<Storage>(cbfs_drive, file_info));
  LOG(kInfo) << "CbFsFlushFile - " << relative_path;
  try {
    cbfs_drive->Flush(path);
  }
  catch (const drive_error& error) {
    LOG(kError) << "CbFsFlushFile: " << relative_path << ": " << error.what();
    if (error.code() == make_error_code(DriveErrors::no_such_file)
      throw ECBFSError(ERROR_FILE_NOT_FOUND);
    else
      throw ECBFSError(ERROR_ERRORS_ENCOUNTERED);
  }
  catch (const std::exception& e) {
    LOG(kError) << "CbFsFlushFile: " << relative_path << ": " << e.what();
    throw ECBFSError(ERROR_ERRORS_ENCOUNTERED);
  }
}

template <typename Storage>
void CbfsDrive<Storage>::CbFsOnEjectStorage(CallbackFileSystem* sender) {
  LOG(kInfo) << "CbFsOnEjectStorage";
  auto cbfs_drive(GetDrive(sender));
  boost::async(boost::launch::async, [cbfs_drive] { cbfs_drive->Unmount(); });
}

template <typename Storage>
void CbfsDrive<Storage>::SetNewAttributes(FileContextPtr file_context, bool is_directory,
                                          bool read_only) {
  SCOPED_PROFILE
  FILETIME file_time;
  GetSystemTimeAsFileTime(&file_time);
  file_context->meta_data.creation_time = file_context->meta_data.last_access_time =
      file_context->meta_data.last_write_time = file_time;

  if (is_directory) {
    file_context->meta_data.attributes = FILE_ATTRIBUTE_DIRECTORY;
  } else {
    if (read_only)
      file_context->meta_data.attributes = FILE_ATTRIBUTE_READONLY;
    else
      file_context->meta_data.attributes = FILE_ATTRIBUTE_NORMAL;

    file_context->self_encryptor.reset(new SelfEncryptor(
        file_context->meta_data.data_map, *storage_));
    file_context->meta_data.end_of_file = file_context->meta_data.allocation_size =
        file_context->self_encryptor->size();
  }
}

}  // namespace drive

}  // namespace maidsafe

#endif  // MAIDSAFE_DRIVE_WIN_DRIVE_H_
