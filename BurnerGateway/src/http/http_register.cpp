/*
 * CIP Data Gateway
 *
 * Copyright (c) 2026 Adam G. Sweeney
 *
 * SPDX-License-Identifier: MIT
 * See the LICENSE file in the repository root for full license text.
 */

// Centralized HTTP route registration (single-source).
#ifdef BURNERGATEWAY_MAIN_TU

CallBackFunctionPageHandler gPlcScanHandler("api/plc/scan", HandlePlcScanApi, tGet, 0, true);

CallBackFunctionPageHandler gPlcSaveHandler("api/plc/save/", HandlePlcSaveApi, MatchPlcSaveApi, tGet, 0, true);
CallBackFunctionPageHandler gPlcSelectedHandler("api/plc/selected", HandlePlcSelectedApi, tGet, 0, true);
CallBackFunctionPageHandler gPlcClearHandler("api/plc/clear", HandlePlcClearApi, tGet, 0, true);
CallBackFunctionPageHandler gNetworkConfigSaveHandler("api/network/config/save", HandleNetworkConfigSaveApi, MatchNetworkConfigSaveApi, tGet, 0, true);
CallBackFunctionPageHandler gNetworkConfigHandler("api/network/config", HandleNetworkConfigApi, MatchNetworkConfigApi, tGet, 0, true);
CallBackFunctionPageHandler gReajetConfigHandler("api/reajet/config", HandleReajetConfigApi, MatchReajetConfigApi, tGet, 0, true);
CallBackFunctionPageHandler gReajetConfigSaveHandler("api/reajet/config/save", HandleReajetConfigSaveApi, MatchReajetConfigSaveApi, tGet, 0, true);
CallBackFunctionPageHandler gReajetClearHandler("api/reajet/config/clear", HandleReajetClearApi, tGet, 0, true);
CallBackFunctionPageHandler gReajetStatusHandler("api/reajet/status", HandleReajetStatusApi, MatchReajetStatusApi, tGet, 0, true);
CallBackFunctionPageHandler gReajetManualStatusHandler("api/reajet/manual_status", HandleReajetManualStatusApi, MatchReajetManualStatusApi, tGet, 0, true);
CallBackFunctionPageHandler gPlcReadHandler("api/plc/read", HandlePlcReadApi, tGet, 0, true);
CallBackFunctionPageHandler gReajetSendHandler("api/reajet/send", HandleReajetSendApi, tGet, 0, true);
CallBackFunctionPageHandler gMappingsListHandler("api/mappings/list", HandleMappingsListApi, tGet, 0, true);
CallBackFunctionPageHandler gMappingsSaveHandler("api/mappings/save", HandleMappingsSaveApi, MatchMappingsSaveApi, tGet, 0, true);
CallBackFunctionPageHandler gMappingsDeleteHandler("api/mappings/delete", HandleMappingsDeleteApi, MatchMappingsDeleteApi, tGet, 0, true);
CallBackFunctionPageHandler gMappingsClearHandler("api/mappings/clear", HandleMappingsClearApi, MatchMappingsClearApi, tGet, 0, true);
CallBackFunctionPageHandler gMappingsRuntimeHandler("api/mappings/runtime", HandleMappingsRuntimeApi, MatchMappingsRuntimeApi, tGet, 0, true);

CallBackFunctionPageHandler gPlcTagsListHandler("api/plc/tags", HandlePlcTagsListApi, tGet, 0, true);
CallBackFunctionPageHandler gPlcTagsAddHandler("api/plc/tags/add", HandlePlcTagsAddApi, MatchPlcTagsAddApi, tGet, 0, true);
CallBackFunctionPageHandler gPlcTagsAddNoSaveHandler("api/plc/tags/add_nosave", HandlePlcTagsAddNoSaveApi, MatchPlcTagsAddNoSaveApi, tGet, 0, true);
CallBackFunctionPageHandler gPlcTagsCommitHandler("api/plc/tags/commit", HandlePlcTagsCommitApi, tGet, 0, true);
CallBackFunctionPageHandler gPlcTagsClearHandler("api/plc/tags/clear", HandlePlcTagsClearApi, tGet, 0, true);
CallBackFunctionPageHandler gPlcTagsImportAddNoSaveHandler("api/plc/tags/import/add_nosave", HandlePlcTagsImportAddNoSaveApi, MatchPlcTagsImportAddNoSaveApi, tGet, 0, true);
CallBackFunctionPageHandler gPlcTagsImportCommitHandler("api/plc/tags/import/commit", HandlePlcTagsImportCommitApi, tGet, 0, true);
CallBackFunctionPageHandler gPlcTagsImportClearHandler("api/plc/tags/import/clear", HandlePlcTagsImportClearApi, tGet, 0, true);
CallBackFunctionPageHandler gPlcTagsExpandHandler("api/plc/tags/expand", HandlePlcTagsExpandApi, MatchPlcTagsExpandApi, tGet, 0, true);
CallBackFunctionPageHandler gPlcTagsRemoveHandler("api/plc/tags/remove/", HandlePlcTagsRemoveApi, MatchPlcTagsRemoveApi, tGet, 0, true);
CallBackFunctionPageHandler gPlcTagsBrowseHandler("api/plc/tags/browse", HandlePlcTagsBrowseApi, tGet, 0, true);

#endif // BURNERGATEWAY_MAIN_TU
