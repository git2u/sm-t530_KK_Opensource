// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_data_manager_impl_private.h"

#if defined(OS_MACOSX)
#include <ApplicationServices/ApplicationServices.h>
#endif  // OS_MACOSX

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/sys_info.h"
#include "base/version.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/gpu/gpu_util.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/gpu/gpu_info_collector.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/gpu_feature_type.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "grit/content_resources.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_switching_manager.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/plugins/plugin_switches.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace content {
namespace {

// Strip out the non-digital info; if after that, we get an empty string,
// return "0".
std::string ProcessVersionString(const std::string& raw_string) {
  const std::string valid_set = "0123456789.";
  size_t start_pos = raw_string.find_first_of(valid_set);
  if (start_pos == std::string::npos)
    return "0";
  size_t end_pos = raw_string.find_first_not_of(raw_string, start_pos);
  std::string version_string = raw_string.substr(
      start_pos, end_pos - start_pos);
  if (version_string.empty())
    return "0";
  return version_string;
}

// Combine the integers into a string, seperated by ','.
std::string IntSetToString(const std::set<int>& list) {
  std::string rt;
  for (std::set<int>::const_iterator it = list.begin();
       it != list.end(); ++it) {
    if (!rt.empty())
      rt += ",";
    rt += base::IntToString(*it);
  }
  return rt;
}

#if defined(OS_MACOSX)
void DisplayReconfigCallback(CGDirectDisplayID display,
                             CGDisplayChangeSummaryFlags flags,
                             void* gpu_data_manager) {
  if (flags & kCGDisplayAddFlag) {
    GpuDataManagerImpl* manager =
        reinterpret_cast<GpuDataManagerImpl*>(gpu_data_manager);
    DCHECK(manager);
    manager->HandleGpuSwitch();
  }
}
#endif  // OS_MACOSX

// Block all domains' use of 3D APIs for this many milliseconds if
// approaching a threshold where system stability might be compromised.
const int64 kBlockAllDomainsMs = 10000;
const int kNumResetsWithinDuration = 1;

// Enums for UMA histograms.
enum BlockStatusHistogram {
  BLOCK_STATUS_NOT_BLOCKED,
  BLOCK_STATUS_SPECIFIC_DOMAIN_BLOCKED,
  BLOCK_STATUS_ALL_DOMAINS_BLOCKED,
  BLOCK_STATUS_MAX
};

}  // namespace anonymous

void GpuDataManagerImplPrivate::InitializeForTesting(
    const std::string& gpu_blacklist_json,
    const GPUInfo& gpu_info) {
  // This function is for testing only, so disable histograms.
  update_histograms_ = false;

  InitializeImpl(gpu_blacklist_json, std::string(), std::string(), gpu_info);
}

bool GpuDataManagerImplPrivate::IsFeatureBlacklisted(int feature) const {
  if (use_swiftshader_) {
    // Skia's software rendering is probably more efficient than going through
    // software emulation of the GPU, so use that.
    if (feature == GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS)
      return true;
    return false;
  }

  return (blacklisted_features_.count(feature) == 1);
}

size_t GpuDataManagerImplPrivate::GetBlacklistedFeatureCount() const {
  if (use_swiftshader_)
    return 1;
  return blacklisted_features_.size();
}

GPUInfo GpuDataManagerImplPrivate::GetGPUInfo() const {
  return gpu_info_;
}

void GpuDataManagerImplPrivate::GetGpuProcessHandles(
    const GpuDataManager::GetGpuProcessHandlesCallback& callback) const {
  GpuProcessHost::GetProcessHandles(callback);
}

bool GpuDataManagerImplPrivate::GpuAccessAllowed(
    std::string* reason) const {
  if (use_swiftshader_)
    return true;

  if (!gpu_info_.gpu_accessible) {
    if (reason) {
      *reason = "GPU process launch failed.";
    }
    return false;
  }

  if (card_blacklisted_) {
    if (reason) {
      *reason = "GPU access is disabled ";
      CommandLine* command_line = CommandLine::ForCurrentProcess();
      if (command_line->HasSwitch(switches::kDisableGpu))
        *reason += "through commandline switch --disable-gpu.";
      else
        *reason += "in chrome://settings.";
    }
    return false;
  }

  // We only need to block GPU process if more features are disallowed other
  // than those in the preliminary gpu feature flags because the latter work
  // through renderer commandline switches.
  std::set<int> features = preliminary_blacklisted_features_;
  MergeFeatureSets(&features, blacklisted_features_);
  if (features.size() > preliminary_blacklisted_features_.size()) {
    if (reason) {
      *reason = "Features are disabled upon full but not preliminary GPU info.";
    }
    return false;
  }

  if (blacklisted_features_.size() == NUMBER_OF_GPU_FEATURE_TYPES) {
    // On Linux, we use cached GL strings to make blacklist decsions at browser
    // startup time. We need to launch the GPU process to validate these
    // strings even if all features are blacklisted. If all GPU features are
    // disabled, the GPU process will only initialize GL bindings, create a GL
    // context, and collect full GPU info.
#if !defined(OS_LINUX)
    if (reason) {
      *reason = "All GPU features are blacklisted.";
    }
    return false;
#endif
  }

  return true;
}

void GpuDataManagerImplPrivate::RequestCompleteGpuInfoIfNeeded() {
  if (complete_gpu_info_already_requested_ || gpu_info_.finalized)
    return;
  complete_gpu_info_already_requested_ = true;

  GpuProcessHost::SendOnIO(
#if defined(OS_WIN)
      GpuProcessHost::GPU_PROCESS_KIND_UNSANDBOXED,
#else
      GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
#endif
      CAUSE_FOR_GPU_LAUNCH_GPUDATAMANAGER_REQUESTCOMPLETEGPUINFOIFNEEDED,
      new GpuMsg_CollectGraphicsInfo());
}

bool GpuDataManagerImplPrivate::IsCompleteGpuInfoAvailable() const {
  return gpu_info_.finalized;
}

void GpuDataManagerImplPrivate::RequestVideoMemoryUsageStatsUpdate() const {
  GpuProcessHost::SendOnIO(
      GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
      CAUSE_FOR_GPU_LAUNCH_NO_LAUNCH,
      new GpuMsg_GetVideoMemoryUsageStats());
}

bool GpuDataManagerImplPrivate::ShouldUseSwiftShader() const {
  return use_swiftshader_;
}

void GpuDataManagerImplPrivate::RegisterSwiftShaderPath(
    const base::FilePath& path) {
  swiftshader_path_ = path;
  EnableSwiftShaderIfNecessary();
}

void GpuDataManagerImplPrivate::AddObserver(GpuDataManagerObserver* observer) {
  GpuDataManagerImpl::UnlockedSession session(owner_);
  observer_list_->AddObserver(observer);
}

void GpuDataManagerImplPrivate::RemoveObserver(
    GpuDataManagerObserver* observer) {
  GpuDataManagerImpl::UnlockedSession session(owner_);
  observer_list_->RemoveObserver(observer);
}

void GpuDataManagerImplPrivate::UnblockDomainFrom3DAPIs(const GURL& url) {
  // This method must do two things:
  //
  //  1. If the specific domain is blocked, then unblock it.
  //
  //  2. Reset our notion of how many GPU resets have occurred recently.
  //     This is necessary even if the specific domain was blocked.
  //     Otherwise, if we call Are3DAPIsBlocked with the same domain right
  //     after unblocking it, it will probably still be blocked because of
  //     the recent GPU reset caused by that domain.
  //
  // These policies could be refined, but at a certain point the behavior
  // will become difficult to explain.
  std::string domain = GetDomainFromURL(url);

  blocked_domains_.erase(domain);
  timestamps_of_gpu_resets_.clear();
}

void GpuDataManagerImplPrivate::DisableGpuWatchdog() {
  GpuProcessHost::SendOnIO(
      GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
      CAUSE_FOR_GPU_LAUNCH_NO_LAUNCH,
      new GpuMsg_DisableWatchdog);
}

void GpuDataManagerImplPrivate::SetGLStrings(const std::string& gl_vendor,
                                             const std::string& gl_renderer,
                                             const std::string& gl_version) {
  if (gl_vendor.empty() && gl_renderer.empty() && gl_version.empty())
    return;

  // If GPUInfo already got GL strings, do nothing.  This is for the rare
  // situation where GPU process collected GL strings before this call.
  if (!gpu_info_.gl_vendor.empty() ||
      !gpu_info_.gl_renderer.empty() ||
      !gpu_info_.gl_version_string.empty())
    return;

  GPUInfo gpu_info = gpu_info_;

  gpu_info.gl_vendor = gl_vendor;
  gpu_info.gl_renderer = gl_renderer;
  gpu_info.gl_version_string = gl_version;

  gpu_info_collector::CollectDriverInfoGL(&gpu_info);

  UpdateGpuInfo(gpu_info);
  UpdateGpuSwitchingManager(gpu_info);
  UpdatePreliminaryBlacklistedFeatures();
}

void GpuDataManagerImplPrivate::GetGLStrings(std::string* gl_vendor,
                                             std::string* gl_renderer,
                                             std::string* gl_version) {
  DCHECK(gl_vendor && gl_renderer && gl_version);

  *gl_vendor = gpu_info_.gl_vendor;
  *gl_renderer = gpu_info_.gl_renderer;
  *gl_version = gpu_info_.gl_version_string;
}

void GpuDataManagerImplPrivate::Initialize() {
  TRACE_EVENT0("startup", "GpuDataManagerImpl::Initialize");
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kSkipGpuDataLoading))
    return;

  GPUInfo gpu_info;
  {
    TRACE_EVENT0("startup",
      "GpuDataManagerImpl::Initialize:CollectBasicGraphicsInfo");
    gpu_info_collector::CollectBasicGraphicsInfo(&gpu_info);
  }
#if defined(ARCH_CPU_X86_FAMILY)
  if (!gpu_info.gpu.vendor_id || !gpu_info.gpu.device_id)
    gpu_info.finalized = true;
#endif

  std::string gpu_blacklist_string;
  std::string gpu_switching_list_string;
  std::string gpu_driver_bug_list_string;
  if (!command_line->HasSwitch(switches::kIgnoreGpuBlacklist)) {
    const base::StringPiece gpu_blacklist_json =
        GetContentClient()->GetDataResource(
            IDR_GPU_BLACKLIST, ui::SCALE_FACTOR_NONE);
    gpu_blacklist_string = gpu_blacklist_json.as_string();
    const base::StringPiece gpu_switching_list_json =
        GetContentClient()->GetDataResource(
            IDR_GPU_SWITCHING_LIST, ui::SCALE_FACTOR_NONE);
    gpu_switching_list_string = gpu_switching_list_json.as_string();
  }
  if (!command_line->HasSwitch(switches::kDisableGpuDriverBugWorkarounds)) {
    const base::StringPiece gpu_driver_bug_list_json =
        GetContentClient()->GetDataResource(
            IDR_GPU_DRIVER_BUG_LIST, ui::SCALE_FACTOR_NONE);
    gpu_driver_bug_list_string = gpu_driver_bug_list_json.as_string();
  }
  InitializeImpl(gpu_blacklist_string,
                 gpu_switching_list_string,
                 gpu_driver_bug_list_string,
                 gpu_info);
}

void GpuDataManagerImplPrivate::UpdateGpuInfo(const GPUInfo& gpu_info) {
  // No further update of gpu_info if falling back to SwiftShader.
  if (use_swiftshader_)
    return;

  gpu_info_collector::MergeGPUInfo(&gpu_info_, gpu_info);
  complete_gpu_info_already_requested_ =
      complete_gpu_info_already_requested_ || gpu_info_.finalized;

  GetContentClient()->SetGpuInfo(gpu_info_);

  if (gpu_blacklist_) {
    std::set<int> features = gpu_blacklist_->MakeDecision(
        GpuControlList::kOsAny, std::string(), gpu_info_);
    if (update_histograms_)
      UpdateStats(gpu_blacklist_.get(), features);

    UpdateBlacklistedFeatures(features);
  }
  if (gpu_switching_list_) {
    std::set<int> option = gpu_switching_list_->MakeDecision(
        GpuControlList::kOsAny, std::string(), gpu_info_);
    if (option.size() == 1) {
      // Blacklist decision should not overwrite commandline switch from users.
      CommandLine* command_line = CommandLine::ForCurrentProcess();
      if (!command_line->HasSwitch(switches::kGpuSwitching))
        gpu_switching_ = static_cast<GpuSwitchingOption>(*(option.begin()));
    }
  }
  if (gpu_driver_bug_list_)
    gpu_driver_bugs_ = gpu_driver_bug_list_->MakeDecision(
        GpuControlList::kOsAny, std::string(), gpu_info_);

  // We have to update GpuFeatureType before notify all the observers.
  NotifyGpuInfoUpdate();
}

void GpuDataManagerImplPrivate::UpdateVideoMemoryUsageStats(
    const GPUVideoMemoryUsageStats& video_memory_usage_stats) {
  GpuDataManagerImpl::UnlockedSession session(owner_);
  observer_list_->Notify(&GpuDataManagerObserver::OnVideoMemoryUsageStatsUpdate,
                         video_memory_usage_stats);
}

void GpuDataManagerImplPrivate::AppendRendererCommandLine(
    CommandLine* command_line) const {
  DCHECK(command_line);

  if (IsFeatureBlacklisted(GPU_FEATURE_TYPE_WEBGL)) {
#if !defined(OS_ANDROID)
    if (!command_line->HasSwitch(switches::kDisableExperimentalWebGL))
      command_line->AppendSwitch(switches::kDisableExperimentalWebGL);
#endif
    if (!command_line->HasSwitch(switches::kDisablePepper3d))
      command_line->AppendSwitch(switches::kDisablePepper3d);
  }
  if (IsFeatureBlacklisted(GPU_FEATURE_TYPE_MULTISAMPLING) &&
      !command_line->HasSwitch(switches::kDisableGLMultisampling))
    command_line->AppendSwitch(switches::kDisableGLMultisampling);
  if (IsFeatureBlacklisted(GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING) &&
      !command_line->HasSwitch(switches::kDisableAcceleratedCompositing))
    command_line->AppendSwitch(switches::kDisableAcceleratedCompositing);
  if (IsFeatureBlacklisted(GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS) &&
      !command_line->HasSwitch(switches::kDisableAccelerated2dCanvas))
    command_line->AppendSwitch(switches::kDisableAccelerated2dCanvas);
  if (IsFeatureBlacklisted(GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE) &&
      !command_line->HasSwitch(switches::kDisableAcceleratedVideoDecode))
    command_line->AppendSwitch(switches::kDisableAcceleratedVideoDecode);
  if (ShouldUseSwiftShader())
    command_line->AppendSwitch(switches::kDisableFlashFullscreen3d);
}

void GpuDataManagerImplPrivate::AppendGpuCommandLine(
    CommandLine* command_line) const {
  DCHECK(command_line);

  std::string use_gl =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(switches::kUseGL);
  base::FilePath swiftshader_path =
      CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kSwiftShaderPath);
  if (IsFeatureBlacklisted(GPU_FEATURE_TYPE_MULTISAMPLING) &&
      !command_line->HasSwitch(switches::kDisableGLMultisampling))
    command_line->AppendSwitch(switches::kDisableGLMultisampling);
  if (IsFeatureBlacklisted(GPU_FEATURE_TYPE_TEXTURE_SHARING))
    command_line->AppendSwitch(switches::kDisableImageTransportSurface);

  if (use_swiftshader_) {
    command_line->AppendSwitchASCII(switches::kUseGL, "swiftshader");
    if (swiftshader_path.empty())
      swiftshader_path = swiftshader_path_;
  } else if ((IsFeatureBlacklisted(GPU_FEATURE_TYPE_WEBGL) ||
              IsFeatureBlacklisted(GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING) ||
              IsFeatureBlacklisted(GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS)) &&
      (use_gl == "any")) {
    command_line->AppendSwitchASCII(
        switches::kUseGL, gfx::kGLImplementationOSMesaName);
  } else if (!use_gl.empty()) {
    command_line->AppendSwitchASCII(switches::kUseGL, use_gl);
  }
  if (ui::GpuSwitchingManager::GetInstance()->SupportsDualGpus()) {
    command_line->AppendSwitchASCII(switches::kSupportsDualGpus, "true");
    switch (gpu_switching_) {
      case GPU_SWITCHING_OPTION_FORCE_DISCRETE:
        command_line->AppendSwitchASCII(switches::kGpuSwitching,
            switches::kGpuSwitchingOptionNameForceDiscrete);
        break;
      case GPU_SWITCHING_OPTION_FORCE_INTEGRATED:
        command_line->AppendSwitchASCII(switches::kGpuSwitching,
            switches::kGpuSwitchingOptionNameForceIntegrated);
        break;
      case GPU_SWITCHING_OPTION_AUTOMATIC:
      case GPU_SWITCHING_OPTION_UNKNOWN:
        break;
    }
  } else {
    command_line->AppendSwitchASCII(switches::kSupportsDualGpus, "false");
  }

  if (!swiftshader_path.empty())
    command_line->AppendSwitchPath(switches::kSwiftShaderPath,
                                   swiftshader_path);

  if (!gpu_driver_bugs_.empty()) {
    command_line->AppendSwitchASCII(switches::kGpuDriverBugWorkarounds,
                                    IntSetToString(gpu_driver_bugs_));
  }

#if defined(OS_WIN)
  // DisplayLink 7.1 and earlier can cause the GPU process to crash on startup.
  // http://crbug.com/177611
  // Thinkpad USB Port Replicator driver causes GPU process to crash when the
  // sandbox is enabled. http://crbug.com/181665.
  if ((gpu_info_.display_link_version.IsValid()
      && gpu_info_.display_link_version.IsOlderThan("7.2")) ||
      gpu_info_.lenovo_dcute) {
    command_line->AppendSwitch(switches::kReduceGpuSandbox);
  }
#endif

  if (gpu_info_.optimus)
    command_line->AppendSwitch(switches::kReduceGpuSandbox);
  if (gpu_info_.amd_switchable) {
    // The image transport surface currently doesn't work with AMD Dynamic
    // Switchable graphics.
    command_line->AppendSwitch(switches::kReduceGpuSandbox);
    command_line->AppendSwitch(switches::kDisableImageTransportSurface);
  }
  // Pass GPU and driver information to GPU process. We try to avoid full GPU
  // info collection at GPU process startup, but we need gpu vendor_id,
  // device_id, driver_vendor, driver_version for deciding whether we need to
  // collect full info (on Linux) and for crash reporting purpose.
  command_line->AppendSwitchASCII(switches::kGpuVendorID,
      base::StringPrintf("0x%04x", gpu_info_.gpu.vendor_id));
  command_line->AppendSwitchASCII(switches::kGpuDeviceID,
      base::StringPrintf("0x%04x", gpu_info_.gpu.device_id));
  command_line->AppendSwitchASCII(switches::kGpuDriverVendor,
      gpu_info_.driver_vendor);
  command_line->AppendSwitchASCII(switches::kGpuDriverVersion,
      gpu_info_.driver_version);
}

void GpuDataManagerImplPrivate::AppendPluginCommandLine(
    CommandLine* command_line) const {
  DCHECK(command_line);

#if defined(OS_MACOSX)
  // TODO(jbauman): Add proper blacklist support for core animation plugins so
  // special-casing this video card won't be necessary. See
  // http://crbug.com/134015
  if (IsFeatureBlacklisted(GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING) ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableAcceleratedCompositing)) {
    if (!command_line->HasSwitch(
           switches::kDisableCoreAnimationPlugins))
      command_line->AppendSwitch(
          switches::kDisableCoreAnimationPlugins);
  }
#endif
}

void GpuDataManagerImplPrivate::UpdateRendererWebPrefs(
    WebPreferences* prefs) const {
  DCHECK(prefs);

  if (IsFeatureBlacklisted(GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING))
    prefs->accelerated_compositing_enabled = false;
  if (IsFeatureBlacklisted(GPU_FEATURE_TYPE_WEBGL))
    prefs->experimental_webgl_enabled = false;
  if (IsFeatureBlacklisted(GPU_FEATURE_TYPE_FLASH3D))
    prefs->flash_3d_enabled = false;
  if (IsFeatureBlacklisted(GPU_FEATURE_TYPE_FLASH_STAGE3D)) {
    prefs->flash_stage3d_enabled = false;
    prefs->flash_stage3d_baseline_enabled = false;
  }
  if (IsFeatureBlacklisted(GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE))
    prefs->flash_stage3d_baseline_enabled = false;
  if (IsFeatureBlacklisted(GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS))
    prefs->accelerated_2d_canvas_enabled = false;
  if (IsFeatureBlacklisted(GPU_FEATURE_TYPE_MULTISAMPLING))
    prefs->gl_multisampling_enabled = false;
  if (IsFeatureBlacklisted(GPU_FEATURE_TYPE_3D_CSS)) {
    prefs->accelerated_compositing_for_3d_transforms_enabled = false;
    prefs->accelerated_compositing_for_animation_enabled = false;
  }
  if (IsFeatureBlacklisted(GPU_FEATURE_TYPE_ACCELERATED_VIDEO))
    prefs->accelerated_compositing_for_video_enabled = false;

  // Accelerated video and animation are slower than regular when using
  // SwiftShader. 3D CSS may also be too slow to be worthwhile.
  if (ShouldUseSwiftShader()) {
    prefs->accelerated_compositing_for_video_enabled = false;
    prefs->accelerated_compositing_for_animation_enabled = false;
    prefs->accelerated_compositing_for_3d_transforms_enabled = false;
    prefs->accelerated_compositing_for_plugins_enabled = false;
  }
}

GpuSwitchingOption GpuDataManagerImplPrivate::GetGpuSwitchingOption() const {
  if (!ui::GpuSwitchingManager::GetInstance()->SupportsDualGpus())
    return GPU_SWITCHING_OPTION_UNKNOWN;
  return gpu_switching_;
}

void GpuDataManagerImplPrivate::DisableHardwareAcceleration() {
  card_blacklisted_ = true;

  for (int i = 0; i < NUMBER_OF_GPU_FEATURE_TYPES; ++i)
    blacklisted_features_.insert(i);

  EnableSwiftShaderIfNecessary();
  NotifyGpuInfoUpdate();
}

std::string GpuDataManagerImplPrivate::GetBlacklistVersion() const {
  if (gpu_blacklist_)
    return gpu_blacklist_->version();
  return "0";
}

base::ListValue* GpuDataManagerImplPrivate::GetBlacklistReasons() const {
  ListValue* reasons = new ListValue();
  if (gpu_blacklist_)
    gpu_blacklist_->GetReasons(reasons);
  return reasons;
}

void GpuDataManagerImplPrivate::AddLogMessage(
    int level, const std::string& header, const std::string& message) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger("level", level);
  dict->SetString("header", header);
  dict->SetString("message", message);
  log_messages_.Append(dict);
}

void GpuDataManagerImplPrivate::ProcessCrashed(
    base::TerminationStatus exit_code) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    // Unretained is ok, because it's posted to UI thread, the thread
    // where the singleton GpuDataManagerImpl lives until the end.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&GpuDataManagerImpl::ProcessCrashed,
                   base::Unretained(owner_),
                   exit_code));
    return;
  }
  {
    GpuDataManagerImpl::UnlockedSession session(owner_);
    observer_list_->Notify(
        &GpuDataManagerObserver::OnGpuProcessCrashed, exit_code);
  }
}

base::ListValue* GpuDataManagerImplPrivate::GetLogMessages() const {
  base::ListValue* value;
  value = log_messages_.DeepCopy();
  return value;
}

void GpuDataManagerImplPrivate::HandleGpuSwitch() {
  GpuDataManagerImpl::UnlockedSession session(owner_);
  observer_list_->Notify(&GpuDataManagerObserver::OnGpuSwitching);
}

#if defined(OS_WIN)
bool GpuDataManagerImplPrivate::IsUsingAcceleratedSurface() const {
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return false;

  if (gpu_info_.amd_switchable)
    return false;
  if (use_swiftshader_)
    return false;
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableImageTransportSurface))
    return false;
  return !IsFeatureBlacklisted(GPU_FEATURE_TYPE_TEXTURE_SHARING);
}
#endif

void GpuDataManagerImplPrivate::BlockDomainFrom3DAPIs(
    const GURL& url, GpuDataManagerImpl::DomainGuilt guilt) {
  BlockDomainFrom3DAPIsAtTime(url, guilt, base::Time::Now());
}

bool GpuDataManagerImplPrivate::Are3DAPIsBlocked(const GURL& url,
                                                 int render_process_id,
                                                 int render_view_id,
                                                 ThreeDAPIType requester) {
  bool blocked = Are3DAPIsBlockedAtTime(url, base::Time::Now()) !=
      GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_NOT_BLOCKED;
  if (blocked) {
    // Unretained is ok, because it's posted to UI thread, the thread
    // where the singleton GpuDataManagerImpl lives until the end.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&GpuDataManagerImpl::Notify3DAPIBlocked,
                   base::Unretained(owner_), url, render_process_id,
                   render_view_id, requester));
  }

  return blocked;
}

void GpuDataManagerImplPrivate::DisableDomainBlockingFor3DAPIsForTesting() {
  domain_blocking_enabled_ = false;
}

// static
GpuDataManagerImplPrivate* GpuDataManagerImplPrivate::Create(
    GpuDataManagerImpl* owner) {
  return new GpuDataManagerImplPrivate(owner);
}

GpuDataManagerImplPrivate::GpuDataManagerImplPrivate(
    GpuDataManagerImpl* owner)
    : complete_gpu_info_already_requested_(false),
      gpu_switching_(GPU_SWITCHING_OPTION_AUTOMATIC),
      observer_list_(new GpuDataManagerObserverList),
      use_swiftshader_(false),
      card_blacklisted_(false),
      update_histograms_(true),
      window_count_(0),
      domain_blocking_enabled_(true),
      owner_(owner) {
  DCHECK(owner_);
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableAcceleratedCompositing)) {
    command_line->AppendSwitch(switches::kDisableAccelerated2dCanvas);
    command_line->AppendSwitch(switches::kDisableAcceleratedLayers);
  }
  if (command_line->HasSwitch(switches::kDisableGpu))
    DisableHardwareAcceleration();
  if (command_line->HasSwitch(switches::kGpuSwitching)) {
    std::string option_string = command_line->GetSwitchValueASCII(
        switches::kGpuSwitching);
    GpuSwitchingOption option = StringToGpuSwitchingOption(option_string);
    if (option != GPU_SWITCHING_OPTION_UNKNOWN)
      gpu_switching_ = option;
  }

#if defined(OS_MACOSX)
  CGDisplayRegisterReconfigurationCallback(DisplayReconfigCallback, owner_);
#endif  // OS_MACOSX
}

GpuDataManagerImplPrivate::~GpuDataManagerImplPrivate() {
#if defined(OS_MACOSX)
  CGDisplayRemoveReconfigurationCallback(DisplayReconfigCallback, owner_);
#endif
}

void GpuDataManagerImplPrivate::InitializeImpl(
    const std::string& gpu_blacklist_json,
    const std::string& gpu_switching_list_json,
    const std::string& gpu_driver_bug_list_json,
    const GPUInfo& gpu_info) {
  std::string browser_version_string = ProcessVersionString(
      GetContentClient()->GetProduct());
  CHECK(!browser_version_string.empty());

  if (!gpu_blacklist_json.empty()) {
    gpu_blacklist_.reset(GpuBlacklist::Create());
    gpu_blacklist_->LoadList(
        browser_version_string, gpu_blacklist_json,
        GpuControlList::kCurrentOsOnly);
  }
  if (!gpu_switching_list_json.empty()) {
    gpu_switching_list_.reset(GpuSwitchingList::Create());
    gpu_switching_list_->LoadList(
        browser_version_string, gpu_switching_list_json,
        GpuControlList::kCurrentOsOnly);
  }
  if (!gpu_driver_bug_list_json.empty()) {
    gpu_driver_bug_list_.reset(GpuDriverBugList::Create());
    gpu_driver_bug_list_->LoadList(
        browser_version_string, gpu_driver_bug_list_json,
        GpuControlList::kCurrentOsOnly);
  }

  gpu_info_ = gpu_info;
  UpdateGpuInfo(gpu_info);
  UpdateGpuSwitchingManager(gpu_info);
  UpdatePreliminaryBlacklistedFeatures();
}

void GpuDataManagerImplPrivate::UpdateBlacklistedFeatures(
    const std::set<int>& features) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  blacklisted_features_ = features;

  // Force disable using the GPU for these features, even if they would
  // otherwise be allowed.
  if (card_blacklisted_ ||
      command_line->HasSwitch(switches::kBlacklistAcceleratedCompositing)) {
    blacklisted_features_.insert(GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING);
  }
  if (card_blacklisted_ ||
      command_line->HasSwitch(switches::kBlacklistWebGL)) {
    blacklisted_features_.insert(GPU_FEATURE_TYPE_WEBGL);
  }

  EnableSwiftShaderIfNecessary();
}

void GpuDataManagerImplPrivate::UpdatePreliminaryBlacklistedFeatures() {
  preliminary_blacklisted_features_ = blacklisted_features_;
}

void GpuDataManagerImplPrivate::UpdateGpuSwitchingManager(
    const GPUInfo& gpu_info) {
  ui::GpuSwitchingManager::GetInstance()->SetGpuCount(
      gpu_info.secondary_gpus.size() + 1);

  if (ui::GpuSwitchingManager::GetInstance()->SupportsDualGpus()) {
    switch (gpu_switching_) {
      case GPU_SWITCHING_OPTION_FORCE_DISCRETE:
        ui::GpuSwitchingManager::GetInstance()->ForceUseOfDiscreteGpu();
        break;
      case GPU_SWITCHING_OPTION_FORCE_INTEGRATED:
        ui::GpuSwitchingManager::GetInstance()->ForceUseOfIntegratedGpu();
        break;
      case GPU_SWITCHING_OPTION_AUTOMATIC:
      case GPU_SWITCHING_OPTION_UNKNOWN:
        break;
    }
  }
}

void GpuDataManagerImplPrivate::NotifyGpuInfoUpdate() {
  observer_list_->Notify(&GpuDataManagerObserver::OnGpuInfoUpdate);
}

void GpuDataManagerImplPrivate::EnableSwiftShaderIfNecessary() {
  if (!GpuAccessAllowed(NULL) ||
      blacklisted_features_.count(GPU_FEATURE_TYPE_WEBGL)) {
    if (!swiftshader_path_.empty() &&
        !CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kDisableSoftwareRasterizer))
      use_swiftshader_ = true;
  }
}

std::string GpuDataManagerImplPrivate::GetDomainFromURL(
    const GURL& url) const {
  // For the moment, we just use the host, or its IP address, as the
  // entry in the set, rather than trying to figure out the top-level
  // domain. This does mean that a.foo.com and b.foo.com will be
  // treated independently in the blocking of a given domain, but it
  // would require a third-party library to reliably figure out the
  // top-level domain from a URL.
  if (!url.has_host()) {
    return std::string();
  }

  return url.host();
}

void GpuDataManagerImplPrivate::BlockDomainFrom3DAPIsAtTime(
    const GURL& url,
    GpuDataManagerImpl::DomainGuilt guilt,
    base::Time at_time) {
  if (!domain_blocking_enabled_)
    return;

  std::string domain = GetDomainFromURL(url);

  DomainBlockEntry& entry = blocked_domains_[domain];
  entry.last_guilt = guilt;
  timestamps_of_gpu_resets_.push_back(at_time);
}

GpuDataManagerImpl::DomainBlockStatus
GpuDataManagerImplPrivate::Are3DAPIsBlockedAtTime(
    const GURL& url, base::Time at_time) const {
  if (!domain_blocking_enabled_)
    return GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_NOT_BLOCKED;

  // Note: adjusting the policies in this code will almost certainly
  // require adjusting the associated unit tests.
  std::string domain = GetDomainFromURL(url);

  DomainBlockMap::const_iterator iter = blocked_domains_.find(domain);
  if (iter != blocked_domains_.end()) {
    // Err on the side of caution, and assume that if a particular
    // domain shows up in the block map, it's there for a good
    // reason and don't let its presence there automatically expire.

    UMA_HISTOGRAM_ENUMERATION("GPU.BlockStatusForClient3DAPIs",
                              BLOCK_STATUS_SPECIFIC_DOMAIN_BLOCKED,
                              BLOCK_STATUS_MAX);

    return GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_BLOCKED;
  }

  // Look at the timestamps of the recent GPU resets to see if there are
  // enough within the threshold which would cause us to blacklist all
  // domains. This doesn't need to be overly precise -- if time goes
  // backward due to a system clock adjustment, that's fine.
  //
  // TODO(kbr): make this pay attention to the TDR thresholds in the
  // Windows registry, but make sure it continues to be testable.
  {
    std::list<base::Time>::iterator iter = timestamps_of_gpu_resets_.begin();
    int num_resets_within_timeframe = 0;
    while (iter != timestamps_of_gpu_resets_.end()) {
      base::Time time = *iter;
      base::TimeDelta delta_t = at_time - time;

      // If this entry has "expired", just remove it.
      if (delta_t.InMilliseconds() > kBlockAllDomainsMs) {
        iter = timestamps_of_gpu_resets_.erase(iter);
        continue;
      }

      ++num_resets_within_timeframe;
      ++iter;
    }

    if (num_resets_within_timeframe >= kNumResetsWithinDuration) {
      UMA_HISTOGRAM_ENUMERATION("GPU.BlockStatusForClient3DAPIs",
                                BLOCK_STATUS_ALL_DOMAINS_BLOCKED,
                                BLOCK_STATUS_MAX);

      return GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_ALL_DOMAINS_BLOCKED;
    }
  }

  UMA_HISTOGRAM_ENUMERATION("GPU.BlockStatusForClient3DAPIs",
                            BLOCK_STATUS_NOT_BLOCKED,
                            BLOCK_STATUS_MAX);

  return GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_NOT_BLOCKED;
}

int64 GpuDataManagerImplPrivate::GetBlockAllDomainsDurationInMs() const {
  return kBlockAllDomainsMs;
}

void GpuDataManagerImplPrivate::Notify3DAPIBlocked(const GURL& url,
                                                   int render_process_id,
                                                   int render_view_id,
                                                   ThreeDAPIType requester) {
  GpuDataManagerImpl::UnlockedSession session(owner_);
  observer_list_->Notify(&GpuDataManagerObserver::DidBlock3DAPIs,
                         url, render_process_id, render_view_id, requester);
}

}  // namespace content

