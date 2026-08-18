#pragma once

// Stable public facade for the Google Drive plugin. Focused collaborators live
// in google_drive_plugin/ while callers keep this include path.
#include "files/infrastructure/native_filesystem.hpp"
#include "google_drive/infrastructure/google_drive_plugin/drive_file_system.hpp"
#include "google_drive/infrastructure/google_drive_plugin/drive_manifest_repository.hpp"
#include "google_drive/infrastructure/google_drive_plugin/drive_metadata_extractor.hpp"
#include "google_drive/infrastructure/google_drive_plugin/drive_search_index_repository.hpp"
#include "google_drive/infrastructure/google_drive_plugin/plugin.hpp"
#include "google_drive/infrastructure/google_drive_plugin/testing.hpp"

namespace crumb::plugins::google_drive::api {
using FileSystem = DriveFileSystem;
using ManifestRepository = DriveManifestRepository;
using MetadataExtractor = DriveMetadataExtractor;
using NativeFileSystem = infrastructure::NativeFileSystem;
using Plugin = GoogleDrivePlugin;
using SearchIndexRepository = DriveSearchIndexRepository;
using ForkFunction = testing::ForkFunction;
using PipeFunction = testing::PipeFunction;
namespace testing = ::crumb::plugins::google_drive::testing;
}  // namespace crumb::plugins::google_drive::api
