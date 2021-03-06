#include <a_http>
#include <a_json>

#define MAX_YOUTUBE_SAVES	10
#define INVALID_YT_ID		-1
#define INVALID_PLAYLIST_ID -1
#define MAX_PLAYLISTS  2

#define MAX_SEARCH_RESULTS  10

enum e_ytv
{
	Admin[24],
	bool:Playing,
	Duration,
	Title[256],
	Link[256],
	StreamLink[256],
	PlaylistID,
	Timer
};

new Youtube[MAX_YOUTUBE_SAVES][e_ytv];

enum e_plist
{
    bool:Active,
    Name[32]
};

new Playlist[MAX_PLAYLISTS][e_plist];

enum pSearchResult
{
	Title[256],
	Link[256]
};

new SearchResults[MAX_PLAYERS][MAX_SEARCH_RESULTS][pSearchResult];

forward OnYoutubeVideoFinished(youtubeid);
forward SongFinished(youtubeid);
forward OnVpsResponse(index, response_code, data[]);
forward OnSearchResponse(index, response_code, data[]);
forward OnMVYoutubeError(youtubeid, message[]);
forward OnYoutubeVideoStart(youtubeid);
forward OnPlaylistAddEntry(playerlistid, youtubeid);
forward OnPlaylistFinished(playlistid);
forward OnYoutubeSearch(playerid);


stock PlayYoutubeVideoFor(playerid, url[], playlist = INVALID_PLAYLIST_ID)
{
	new id = GetFreeIndex(), requestlink[128];
	if(id != INVALID_YT_ID)
	{
	    GetPlayerName(playerid, Youtube[id][Admin], 24);
		Youtube[id][PlaylistID] = playlist;

		if(strfind(url,"&") != -1) strmid(url,url,0,strfind(url,"&"), 128);
		format(Youtube[id][Link],256,"%s",url);
		format(requestlink,sizeof(requestlink),"michaelbelgium.me/ytconverter/convert.php?youtubelink=%s",Youtube[id][Link]);
		HTTP(id, HTTP_GET, requestlink, "", "OnVpsResponse");
	}
	return id;
}

stock SearchYoutubeVideos(playerid, searchquery[])
{
	new requestlink[256];
	format(requestlink, sizeof(requestlink), "michaelbelgium.me/ytconverter/search.php?q=%s&max_results=%i", searchquery, MAX_SEARCH_RESULTS);
	HTTP(playerid, HTTP_GET, requestlink, "", "OnSearchResponse");
}

stock GetFreeIndex()
{
	for(new i = 0; i < MAX_YOUTUBE_SAVES; i++)
	{
		if(!Youtube[i][Playing] && Youtube[i][PlaylistID] == INVALID_PLAYLIST_ID) return i;
	}
	return INVALID_YT_ID;
}

stock StopYoutubeVideo(youtubeid)
{
	Youtube[youtubeid][Playing] = false;
	KillTimer(Youtube[youtubeid][Timer]);
	for(new i = 0, j = GetPlayerPoolSize(); i <= j; i++)
	{
		if(IsPlayerConnected(i))
		{
			StopAudioStreamForPlayer(i);
		}
	}
}

stock IsValidYoutubeURL(string[])			return (strfind(string,"youtube.com") != -1 && strfind(string,"watch?v=") != -1);
stock IsYouTubeVideoPlaying(youtubeid)		return youtubeid == INVALID_YT_ID ? false : Youtube[youtubeid][Playing];
stock GetPlaylistFromVideo(youtubeid)		return youtubeid == INVALID_YT_ID ? INVALID_PLAYLIST_ID : Youtube[youtubeid][PlaylistID];
stock GetVideoDuration(youtubeid)			return youtubeid == INVALID_YT_ID ? 0 : Youtube[youtubeid][Duration];
stock GetVideoTitle(youtubeid)
{
    new str[256];
    strins(str, Youtube[youtubeid][Title], 0);
    return str;
}

stock GetVideoLink(youtubeid)
{
    new str[256];
    strins(str, Youtube[youtubeid][Link], 0);
    return str;
}

stock GetVideoStreamLink(youtubeid)
{
	new str[256];
	strins(str, Youtube[youtubeid][StreamLink], 0);
	return str;
}

stock GetVideoAdmin(youtubeid)
{
    new str[24];
    strins(str, Youtube[youtubeid][Admin], 0);
    return str;
}
stock CreatePlaylist(name[])
{
	for(new i = 0; i < MAX_PLAYLISTS; i++)
	{
		if(Playlist[i][Active]) continue;
		Playlist[i][Active] = true;
		format(Playlist[i][Name], 32, "%s", name);
		return i;
	}
	return INVALID_PLAYLIST_ID;
}

stock RemovePlaylist(playlistid)
{
	if(playlistid != INVALID_PLAYLIST_ID)
	{
		Playlist[playlistid][Active] = false;
		Playlist[playlistid][Name] = EOS;

		for(new i = 0; i < MAX_YOUTUBE_SAVES; i++)
			if(Youtube[i][PlaylistID] == playlistid)
				Youtube[i][PlaylistID] = INVALID_PLAYLIST_ID;
	}
}

stock GetPlaylistName(playlistid)
{
	new str[32];
	strins(str, Playlist[playlistid][Name], 0);
	return str;
}

stock GetPlaylistSongs(playlistid)
{
	new songs[MAX_YOUTUBE_SAVES], pointer = 0;

	for(new i = 0; i < MAX_YOUTUBE_SAVES; i++) songs[i] = INVALID_YT_ID;
	for(new i = 0; i < MAX_YOUTUBE_SAVES; i++)
	{
		if(Youtube[i][PlaylistID] != playlistid) continue;
		songs[pointer] = i;
		pointer++;
	}
	return songs;
}

stock GetPlaylistSongsCount(playlistid)
{
	new count = 0;
	for(new i = 0; i < MAX_YOUTUBE_SAVES; i++)
	{
		if(Youtube[i][PlaylistID] != playlistid) continue;
		count++;
	}
	return count;
}

PlayYoutubeVideo(youtubeid)
{
	Youtube[youtubeid][Playing] = true;
	CallLocalFunction("OnYoutubeVideoStart", "i", youtubeid);

	for(new i = 0, j = GetPlayerPoolSize(); i <= j; i++)
	{
		if(IsPlayerConnected(i))
		{
			PlayAudioStreamForPlayer(i, Youtube[youtubeid][StreamLink]);
		}
	}
	Youtube[youtubeid][Timer] = SetTimerEx("SongFinished",GetVideoDuration(youtubeid)*1000,false,"i",youtubeid);
}

public OnVpsResponse(index, response_code, data[])
{
	new string[256];

	if(response_code != 200)
	{
		format(string, sizeof(string), "%s - response code: %i", GetError(response_code), response_code);
		CallLocalFunction("OnMVYoutubeError", "is", index, string);
		return 0;
	}

	new JSONNode:response = json_parse_string(data);

	if(index == 9999)
	{
		if(json_get_bool(response, "error"))
		{
			json_get_string(response, string, sizeof(string), "message");
			CallLocalFunction("OnMVYoutubeError", "is", index, string);
		}
		return 0;
	}

	if(json_get_bool(response, "error"))
	{
		json_get_string(response, string, sizeof(string), "message");
		CallLocalFunction("OnMVYoutubeError", "is", index, string);
		return 0;
	}

	json_get_string(response, Youtube[index][Title], 256, "title");
	Youtube[index][Duration] = json_get_int(response, "duration");
	json_get_string(response, Youtube[index][StreamLink], 512, "file");
	json_close(response);
	if(Youtube[index][PlaylistID] != INVALID_PLAYLIST_ID)
	{
		CallLocalFunction("OnPlaylistAddEntry", "ii", Youtube[index][PlaylistID], index);

		if(GetPlaylistSongsCount(Youtube[index][PlaylistID]) != 1)
			return 0;
	}
	PlayYoutubeVideo(index);
	return 1;
}
public OnSearchResponse(index, response_code, data[])
{
	new JSONNode:response = json_parse_string(data), JSONArray:results, string[128];
	if(response_code != 200)
	{
		format(string, sizeof(string), "%s - response code: %i", GetError(response_code), response_code);
		CallLocalFunction("OnMVYoutubeError", "is", index, string);
		return 0;
	}
	if(json_get_bool(response, "error"))
	{
		json_get_string(response, string, sizeof(string), "message");
		CallLocalFunction("OnMVYoutubeError", "is", index, string);
		return 0;
	}
	results = json_get_array(response, "results");
	new title[512], link[256];
	for(new i = 0; i < json_array_count(results); i++)
	{
		new JSONNode:node = json_array_at(results, i);

		json_get_string(node, title, sizeof(title), "title");
		json_get_string(node, link, sizeof(link), "full_link");

		format(SearchResults[index][i][Title], sizeof(title), "%s", title);
		format(SearchResults[index][i][Link], sizeof(link), "%s", link);
	}
	CallLocalFunction("OnYoutubeSearch", "i", index);
	return 1;
}

public SongFinished(youtubeid)
{
	StopYoutubeVideo(youtubeid);
	CallLocalFunction("OnYoutubeVideoFinished","i",youtubeid);

	if(Youtube[youtubeid][PlaylistID] != INVALID_PLAYLIST_ID)
	{
		new plist = Youtube[youtubeid][PlaylistID], bool:finished = true;
		Youtube[youtubeid][PlaylistID] = INVALID_PLAYLIST_ID;

		for(new i = 0; i < MAX_YOUTUBE_SAVES; i++)
		{
			if(Youtube[i][PlaylistID] != plist) continue;
			PlayYoutubeVideo(i);
			finished = false;
			break;
		}
		if(finished)
		{
			CallLocalFunction("OnPlaylistFinished", "i", plist);
		}
	}
}

stock GetError(val)
{
	new error[32];
	switch(val)
	{
		case 1: error = "Bad host";
		case 2: error = "No socket";
		case 3: error = "Can't connect";
		case 4: error = "Can't write";
		case 5: error = "Content too big";
		case 6: error = "Malformed response";
		case 300..308: error = "Redirection";
		case 400..499: error = "Client error";
		case 500..599: error = "Server error";
	}
	return error;
}

public OnGameModeInit()
{
	for(new i = 0; i < MAX_YOUTUBE_SAVES; i++)	Youtube[i][PlaylistID] = INVALID_PLAYLIST_ID;

	#if defined MV_OnGameModeInit
		return MV_OnGameModeInit();
	#else
		return 1;
	#endif
}
#if defined _ALS_OnGameModeInit
	#undef OnGameModeInit
#else
	#define _ALS_OnGameModeInit
#endif

#define OnGameModeInit MV_OnGameModeInit
#if defined MV_OnGameModeInit
	forward MV_OnGameModeInit();
#endif
