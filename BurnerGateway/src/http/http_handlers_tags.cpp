/*
 * CIP Data Gateway
 *
 * Copyright (c) 2026 Adam G. Sweeney
 *
 * SPDX-License-Identifier: MIT
 * See the LICENSE file in the repository root for full license text.
 */

#ifdef BURNERGATEWAY_MAIN_TU
static int HandlePlcTagsListApi(int sock, HTTP_Request &req)
{
    (void)req;
    MergeImportedBrowsePathsIntoBrowseCache();
    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock, "{\"count\":%d,\"tags\":[", gPlcTagCount);
    for (int i = 0; i < gPlcTagCount; ++i)
    {
        if (i > 0)
        {
            fdprintf(sock, ",");
        }
        const bool isArrayElement = strchr(gPlcTags[i], '[') != nullptr;
        const bool isUdtPath = strchr(gPlcTags[i], '.') != nullptr;
        const uint16_t typeCode = LookupConfiguredTagTypeCode(gPlcTags[i]);
        const char *typeName = CipTypeNameFromCode(typeCode);
        fdprintf(sock,
                 "{\"index\":%d,\"path\":\"%s\",\"isArrayElement\":%s,\"isUdtPath\":%s,\"typeCode\":%u,\"typeName\":\"%s\"}",
                 i,
                 gPlcTags[i],
                 isArrayElement ? "true" : "false",
                 isUdtPath ? "true" : "false",
                 typeCode,
                 typeName);
    }
    fdprintf(sock, "]}");
    return 1;
}

static int HandlePlcTagsAddApi(int sock, HTTP_Request &req)
{
    char path[160]{0};
    if (!GetQueryParam(req.pURL, "path", path, sizeof(path)) || !AddPlcTagPath(path))
    {
        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"invalid_or_full\"}");
        return 1;
    }

    SaveTagPathsToConfig(true);
    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock, "{\"ok\":true,\"count\":%d}", gPlcTagCount);
    return 1;
}

static bool MatchPlcTagsAddApi(HTTP_Request &req)
{
    if (req.req != tGet || !req.pURL)
    {
        return false;
    }
    const char *url = req.pURL;
    while (*url == '/')
    {
        ++url;
    }
    const char *prefix = "api/plc/tags/add";
    const size_t prefixLen = strlen(prefix);
    if (strncmp(url, prefix, prefixLen) != 0)
    {
        return false;
    }
    const char tail = url[prefixLen];
    return (tail == '\0' || tail == '?' || tail == '#');
}

static int HandlePlcTagsAddNoSaveApi(int sock, HTTP_Request &req)
{
    char path[160]{0};
    if (!GetQueryParam(req.pURL, "path", path, sizeof(path)) || !AddPlcTagPath(path))
    {
        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"invalid_or_full\"}");
        return 1;
    }

    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock, "{\"ok\":true,\"count\":%d}", gPlcTagCount);
    return 1;
}

static bool MatchPlcTagsAddNoSaveApi(HTTP_Request &req)
{
    if (req.req != tGet || !req.pURL)
    {
        return false;
    }
    const char *url = req.pURL;
    while (*url == '/')
    {
        ++url;
    }
    const char *prefix = "api/plc/tags/add_nosave";
    const size_t prefixLen = strlen(prefix);
    if (strncmp(url, prefix, prefixLen) != 0)
    {
        return false;
    }
    const char tail = url[prefixLen];
    return (tail == '\0' || tail == '?' || tail == '#');
}

static int HandlePlcTagsCommitApi(int sock, HTTP_Request &req)
{
    (void)req;
    SaveTagPathsToConfig(true);
    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock, "{\"ok\":true,\"count\":%d}", gPlcTagCount);
    return 1;
}

static int HandlePlcTagsImportAddNoSaveApi(int sock, HTTP_Request &req)
{
    char path[160]{0};
    if (!GetQueryParam(req.pURL, "path", path, sizeof(path)) || !AddImportedBrowsePath(path))
    {
        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"invalid_or_full\"}");
        return 1;
    }

    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock, "{\"ok\":true,\"count\":%d}", gImportedBrowseTagCount);
    return 1;
}

static bool MatchPlcTagsImportAddNoSaveApi(HTTP_Request &req)
{
    if (req.req != tGet || !req.pURL)
    {
        return false;
    }
    const char *url = req.pURL;
    while (*url == '/')
    {
        ++url;
    }
    const char *prefix = "api/plc/tags/import/add_nosave";
    const size_t prefixLen = strlen(prefix);
    if (strncmp(url, prefix, prefixLen) != 0)
    {
        return false;
    }
    const char tail = url[prefixLen];
    return (tail == '\0' || tail == '?' || tail == '#');
}

static int HandlePlcTagsImportCommitApi(int sock, HTTP_Request &req)
{
    (void)req;
    SaveImportedBrowsePathsToConfig(true);
    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock, "{\"ok\":true,\"count\":%d}", gImportedBrowseTagCount);
    return 1;
}

static int HandlePlcTagsImportClearApi(int sock, HTTP_Request &req)
{
    (void)req;
    ClearImportedBrowsePaths();
    SaveImportedBrowsePathsToConfig(true);
    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock, "{\"ok\":true,\"count\":0}");
    return 1;
}

static int HandlePlcTagsClearApi(int sock, HTTP_Request &req)
{
    (void)req;
    gPlcTagCount = 0;
    for (int i = 0; i < kMaxPlcTags; ++i)
    {
        gPlcTags[i][0] = '\0';
    }
    SaveTagPathsToConfig(true);
    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock, "{\"ok\":true,\"count\":0}");
    return 1;
}

static int HandlePlcTagsExpandApi(int sock, HTTP_Request &req)
{
    char base[160]{0};
    char startText[24]{0};
    char endText[24]{0};
    if (!GetQueryParam(req.pURL, "base", base, sizeof(base)) ||
        !GetQueryParam(req.pURL, "start", startText, sizeof(startText)) ||
        !GetQueryParam(req.pURL, "end", endText, sizeof(endText)))
    {
        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"missing_params\"}");
        return 1;
    }

    char baseNorm[kMaxTagPathLen]{0};
    if (!NormalizeTagPath(base, baseNorm, sizeof(baseNorm)))
    {
        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"invalid_base\"}");
        return 1;
    }

    const int start = atoi(startText);
    const int end = atoi(endText);
    if (start < 0 || end < start || (end - start) > 255)
    {
        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"invalid_range\"}");
        return 1;
    }

    int added = 0;
    for (int i = start; i <= end && gPlcTagCount < kMaxPlcTags; ++i)
    {
        char expanded[kMaxTagPathLen]{0};
        sniprintf(expanded, sizeof(expanded), "%s[%d]", baseNorm, i);
        if (AddPlcTagPath(expanded))
        {
            ++added;
        }
    }
    SaveTagPathsToConfig(true);

    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock, "{\"ok\":true,\"added\":%d,\"count\":%d}", added, gPlcTagCount);
    return 1;
}

static bool MatchPlcTagsExpandApi(HTTP_Request &req)
{
    if (req.req != tGet || !req.pURL)
    {
        return false;
    }
    const char *url = req.pURL;
    while (*url == '/')
    {
        ++url;
    }
    const char *prefix = "api/plc/tags/expand";
    const size_t prefixLen = strlen(prefix);
    if (strncmp(url, prefix, prefixLen) != 0)
    {
        return false;
    }
    const char tail = url[prefixLen];
    return (tail == '\0' || tail == '?' || tail == '#');
}

static bool MatchPlcTagsRemoveApi(HTTP_Request &req)
{
    if (req.req != tGet || !req.pURL)
    {
        return false;
    }
    const char *url = req.pURL;
    while (*url == '/')
    {
        ++url;
    }
    return strncmp(url, "api/plc/tags/remove/", strlen("api/plc/tags/remove/")) == 0;
}

static int HandlePlcTagsRemoveApi(int sock, HTTP_Request &req)
{
    const int idx = ParseTagIndex(req.pURL);
    if (idx < 0 || idx >= gPlcTagCount)
    {
        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"invalid_index\"}");
        return 1;
    }

    for (int i = idx; i < gPlcTagCount - 1; ++i)
    {
        strncpy(gPlcTags[i], gPlcTags[i + 1], kMaxTagPathLen - 1);
        gPlcTags[i][kMaxTagPathLen - 1] = '\0';
    }
    if (gPlcTagCount > 0)
    {
        gPlcTags[gPlcTagCount - 1][0] = '\0';
        --gPlcTagCount;
    }
    SaveTagPathsToConfig(true);

    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock, "{\"ok\":true,\"count\":%d}", gPlcTagCount);
    return 1;
}

static int HandlePlcTagsBrowseApi(int sock, HTTP_Request &req)
{
    (void)req;
    char error[96]{0};
    const int count = BrowsePlcTagsFromStoredTarget(error, sizeof(error));
    const bool browseFailed = (count <= 0 && error[0]);
    MergeImportedBrowsePathsIntoBrowseCache();
    if (browseFailed && gBrowsedTagCount <= 0)
    {
        fdprintf(sock, "HTTP/1.0 502 Bad Gateway\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"%s\"}", error);
        return 1;
    }

    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    if (browseFailed)
    {
        fdprintf(sock, "{\"ok\":true,\"warning\":\"%s\",\"count\":%d,\"tags\":[", error, gBrowsedTagCount);
    }
    else
    {
        fdprintf(sock, "{\"ok\":true,\"count\":%d,\"tags\":[", gBrowsedTagCount);
    }
    for (int i = 0; i < gBrowsedTagCount; ++i)
    {
        if (i > 0)
        {
            fdprintf(sock, ",");
        }
        const uint16_t typeCode = static_cast<uint16_t>(gBrowsedTags[i].symbolType & 0x00FF);
        const char *typeName = gBrowsedTags[i].imported ? "Imported" : CipTypeNameFromCode(typeCode);
        fdprintf(sock,
                 "{\"name\":\"%s\",\"symbolType\":%u,\"typeCode\":%u,\"typeName\":\"%s\",\"elementSize\":%u,\"isArray\":%s,\"arrayLength\":%u}",
                 gBrowsedTags[i].name,
                 gBrowsedTags[i].symbolType,
                 typeCode,
                 typeName,
                 gBrowsedTags[i].elementSize,
                 gBrowsedTags[i].isArray ? "true" : "false",
                 gBrowsedTags[i].arrayLength);
    }
    fdprintf(sock, "]}");
    return 1;
}

#endif // BURNERGATEWAY_MAIN_TU

