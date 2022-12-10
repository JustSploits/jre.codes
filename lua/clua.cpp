#include "clua.h"

void lua_panic(sol::optional<std::string> message) {

	if (message) {
		std::string m = message.value();
		MessageBoxA(0, m.c_str(), ("Lua: panic state"), MB_APPLMODAL | MB_OK);
	}
}

// ----- lua functions -----

int extract_owner(sol::this_state st) {
	sol::state_view lua_state(st);
	sol::table rs = lua_state["debug"]["getinfo"](2, "S");
	std::string source = rs["source"];
	std::string filename = std::filesystem::path(source.substr(1)).filename().string();
	return g_lua.get_script_id(filename);
}

namespace ns_config {
	/*
	config.get(key)
	Returns value of given key or nil if key not found.
	*/
	std::tuple<sol::object, sol::object, sol::object, sol::object> get(sol::this_state s, std::string key) {
		std::tuple<sol::object, sol::object, sol::object, sol::object> retn = std::make_tuple(sol::nil, sol::nil, sol::nil, sol::nil);

		for (auto kv : g_hooks.b)
			if (kv.first == key)
				retn = std::make_tuple(sol::make_object(s, kv.second), sol::nil, sol::nil, sol::nil);

		for (auto kv : g_hooks.c)
			if (kv.first == key)
				retn = std::make_tuple(sol::make_object(s, (int)(kv.second[0] * 255)), sol::make_object(s, (int)(kv.second[1] * 255)), sol::make_object(s, (int)(kv.second[2] * 255)), sol::make_object(s, (int)(kv.second[3] * 255)));

		for (auto kv : g_hooks.f)
			if (kv.first == key)
				retn = std::make_tuple(sol::make_object(s, kv.second), sol::nil, sol::nil, sol::nil);

		for (auto kv : g_hooks.i)
			if (kv.first == key)
				retn = std::make_tuple(sol::make_object(s, kv.second), sol::nil, sol::nil, sol::nil);

		for (auto kv : g_hooks.m)
			if (kv.first == key)
				retn = std::make_tuple(sol::make_object(s, kv.second), sol::nil, sol::nil, sol::nil);

		return retn;
	}

	/*
	config.set(key, value)
	Sets value for key
	*/
	void set_bool(std::string key, bool v) {
		g_hooks.b[key] = v;
	}

	void set_float(std::string key, float v) {
		if (ceilf(v) != v)
			g_hooks.f[key] = v;
		else
			g_hooks.i[key] = (int)v;
	}

	void set_color(std::string key, int r, int g, int b, int a) {
		g_hooks.c[key][0] = r / 255.f;
		g_hooks.c[key][1] = g / 255.f;
		g_hooks.c[key][2] = b / 255.f;
		g_hooks.c[key][3] = a / 255.f;
	}

	void set_multiselect(std::string key, int pos, bool e) {
		g_hooks.m[key][pos] = e;
	}

	void set_int(std::string key, int value) {
		g_hooks.i[key] = value;
	}

	/*
	config.load()
	Loads selected config
	*/
	void load() {
	//	g_config.load();
	}

	/*
	config.save()
	Saves selected config

	*/
	void save() {
	//	g_config.save();
	}
}
namespace ns_cheat {
	void set_event_callback(sol::this_state s, std::string eventname, sol::function func) {
		sol::state_view lua_state(s);
		sol::table rs = lua_state["debug"]["getinfo"](2, ("S"));
		std::string source = rs["source"];
		std::string filename = std::filesystem::path(source.substr(1)).filename().string();

		g_lua_hook.registerHook(eventname, g_lua.get_script_id(filename), func);

		g_notify.add(filename + ": subscribed to event " + eventname  +"\n");
	}

	void run_script(std::string scriptname) {
		int scrid = g_lua.get_script_id(scriptname);
		if (scrid < 0)
			return;

		g_lua.load_script(scrid);
	}

	void reload_active_scripts() {
		g_lua.reload_all_scripts();
	}
}
namespace ns_models {
	/*
	models.get_studio_model(mdl)
	Returns studio model of mdl
	*/
	studiohdr_t* get_studio_model(model_t* mdl) {
		return g_csgo.m_model_info->GetStudioModel(mdl);
	}

	/*
	models.get_model_index(name)
	Returns model index of given name
	*/
	int get_model_index(std::string name) {
		return g_csgo.m_model_info->GetModelIndex(name.c_str());
	}
}
namespace ns_engine {
	std::tuple<int, int> get_screen_size() {
		int w, h;
		g_csgo.m_engine->GetScreenSize(w, h);
		return std::make_tuple(w, h);
	}

	void client_cmd(std::string cmd) {
		g_csgo.m_engine->ExecuteClientCmd(cmd.c_str());
	}
	
	int getping1() {
		float latency = 0.f;
		INetChannel* nci = g_csgo.m_engine->GetNetChannelInfo();
		if (nci) {
			latency = nci->GetAvgLatency(INetChannel::FLOW_INCOMING) + nci->GetAvgLatency(INetChannel::FLOW_OUTGOING);

		}
		static auto cl_updaterate = g_csgo.cl_updaterate;
		latency -= 0.5f / cl_updaterate->GetFloat();

		auto ping = (std::max(0.0f, latency) * 1000.0f);


		return ping;
	}
	// получение пинга тестил пон€л ну икак вроде работает 
	player_info_t get_player_info(int ent) {
		player_info_t p;
		g_csgo.m_engine->GetPlayerInfo(ent, &p);
		return p;
	}
	std::string username()
	{
		return g_cl.m_user;
	}
	int get_player_for_user_id(int userid) {
		return g_csgo.m_engine->GetPlayerForUserID(userid);
	}

	int get_local_player_index() {
		return g_csgo.m_engine->GetLocalPlayer();
	}

	float get_last_timestamp() {
		return g_csgo.m_engine->GetLastTimestamp();
	}

	ang_t get_view_angles() {
		ang_t va;
		g_csgo.m_engine->GetViewAngles(va);
		return va;
	}

	void set_view_angles(ang_t va) {
		g_csgo.m_engine->SetViewAngles(va);
	}

	int get_max_clients() {
		return g_csgo.m_globals->m_max_clients;
	}

	bool is_in_game() {
		return g_csgo.m_engine->IsInGame();
	}

	


	INetChannel* get_net_channel_info() {
		return g_csgo.m_engine->GetNetChannelInfo();
	}

	bool is_paused() {
		return g_csgo.m_engine->IsPaused();
	}

	void execute_client_cmd(std::string cmd) {
		g_csgo.m_engine->ExecuteClientCmd(cmd.c_str());
	}
}
namespace ns_entity_list 
{
	sol::optional <Player*> get_local_player()
	{
		if (!g_csgo.m_engine->IsInGame())
			return sol::optional <Player*>(sol::nullopt);

		return (Player*)g_csgo.m_entlist->GetClientEntity(g_csgo.m_engine->GetLocalPlayer());
	}

	Entity* get_client_entity(int idx) {
		return g_csgo.m_entlist->GetClientEntity(idx);
	}

	int get_highest_entity_index() {
		return g_csgo.m_entlist->GetHighestEntityIndex();
	}

	Entity* get_client_entity_from_handle(ULONG ent) {
		return g_csgo.m_entlist->GetClientEntityFromHandle(ent);
	}
}
namespace ns_utils {
	sol::table get_player_data(player_info_t& p) {
		sol::table t = g_lua.lua.create_table();
		t["name"] = std::string(p.m_name);
		t["guid"] = std::string(p.m_guid);
		t["userid"] = p.m_user_id;
		return t;
	}

	double clamp(double v, double mi, double ma) {
		return std::clamp(v, mi, ma);
	}

	vec2_t world_to_screen(vec3_t pos) {
		vec2_t scr;
		render::WorldToScreen(pos, scr);
		return scr;
	}
}
namespace ns_globals {
	float realtime() {
		return g_csgo.m_globals->m_realtime;
	}

	int framecount() {
		return g_csgo.m_globals->m_frame;
	}

	float absoluteframetime() {
		return g_csgo.m_globals->m_abs_frametime;
	}

	float curtime() {
		return g_csgo.m_globals->m_curtime;
	}

	float frametime() {
		return g_csgo.m_globals->m_frametime;
	}

	int maxclients() {
		return g_csgo.m_globals->m_max_clients;
	}

	int tickcount() {
		return g_csgo.m_globals->m_tick_count;
	}

	float tickinterval() {
		return g_csgo.m_globals->m_interval;
	}
}
namespace ns_trace {
	int get_point_contents(vec3_t pos, int mask) { //€сно уже ансрал тут ененен это не €
		return g_csgo.m_engine_trace->GetPointContents(pos, mask);
	}

	/*std::tuple<float, Player*> trace_ray(vec3_t from, vec3_t to, int mask) {
		Ray ray;
		ITraceFilter filter;
		trace_t trace;
		trace.start = from;
		trace.end = to;
		filter.pSkip1 = G::LocalPlayer;

		g_csgo.m_engine_trace->TraceRay(ray, mask, &filter, &trace);
		return std::make_tuple(trace.fraction, trace.m_pEnt);
	}*/
}
namespace ns_cvar {

	ConVar* find_var(std::string name) {
		return g_csgo.m_cvar->FindVar(FNV1a::get(name));
	}

	void console_color_print(Color color, sol::variadic_args args) {
		std::string txt = "";
		for (auto v : args)
			txt += v;

		g_csgo.m_cvar->ConsoleColorPrintf(color, txt.c_str());
	}

	void console_color_print_rgba(int r, int g, int b, int a, sol::variadic_args args) {
		std::string txt = "";
		for (auto v : args)
			txt += v;

		g_csgo.m_cvar->ConsoleColorPrintf(Color(r, g, b, a), txt.c_str());
	}

	void console_print(sol::variadic_args args) {
		std::string txt = "";
		for (auto v : args)
			txt += v;

		g_csgo.m_cvar->ConsoleColorPrintf(Color(255,255,255), txt.c_str());
	}

	void unlock_cvar(ConVar* var)
	{
		if (var)
		{
			var->m_flags &= ~FCVAR_DEVELOPMENTONLY;
			var->m_flags &= ~FCVAR_HIDDEN;
		}
	}

	void remove_callbacks(ConVar* var) {
		if (var)
			var->m_callbacks.RemoveAll();
	}

	float get_float(ConVar* var) {
		return var ? var->GetFloat() : 0.f;
	}

	int get_int(ConVar* var) {
		return var ? var->GetInt() : 0;
	}

	const char* get_string(ConVar* var) {
		return var ? var->GetString() : "";
	}

	void set_float(ConVar* var, float f, bool unlock = false) {
		if (var) var->SetValue(std::to_string(f).c_str());
	}

	void set_int(ConVar* var, int i, bool unlock = false) {
		if (var) var->SetValue(std::to_string(i).c_str());
	}

	void set_string(ConVar* var, const char* v, bool unlock = false) {
		if (var)
			var->SetValue(v);
	}
}
namespace ns_overlay {
	void add_box_overlay(vec3_t pos, vec3_t mins, vec3_t maxs, ang_t orientation, int r, int g, int b, int a, float duration) {
		g_csgo.m_debug_overlay->AddBoxOverlay(pos, mins, maxs, orientation, r, g, b, a, duration);
	}

	void add_text_overlay(vec3_t pos, int line_offset, float duration, sol::variadic_args txt) {
		std::string text = "";
		for (auto v : txt)
			text += v;

		g_csgo.m_debug_overlay->AddTextOverlay(pos, duration, text.c_str());
	}

	void add_capsule_overlay(vec3_t mins, vec3_t maxs, float pill, int r, int g, int b, int a, float duration) {
		g_csgo.m_debug_overlay->AddCapsuleOverlay(mins, maxs, pill, r, g, b, a, duration, 0, 0);
	}
}
namespace ns_beams {
	void draw_beam(Beam_t* beam) {
		g_csgo.m_beams->DrawBeam(beam);
	}

	Beam_t* create_points(BeamInfo_t beam) {
		return g_csgo.m_beams->CreateBeamPoints(beam);
	}
}

namespace ns_ui {
	std::string new_checkbox(sol::this_state s, std::string tab, std::string container, std::string label, std::string key, std::optional<bool> def, std::optional<sol::function> cb) {
		std::transform(tab.begin(), tab.end(), tab.begin(), ::tolower);
		std::transform(container.begin(), container.end(), container.begin(), ::tolower);

		MenuItem_t item;
		item.type = MENUITEM_CHECKBOX;
		item.script = extract_owner(s);
		item.label = label;
		item.key = key;
		item.b_default = def.value_or(false);
		item.callback = cb.value_or(sol::nil);

		g_lua.menu_items[tab][container].push_back(item);
		return key;
	}

	std::string new_slider_int(sol::this_state s, std::string tab, std::string container, std::string label, std::string key, int min, int max, std::optional<std::string> format, std::optional<int> def, std::optional<sol::function> cb) {
		std::transform(tab.begin(), tab.end(), tab.begin(), ::tolower);
		std::transform(container.begin(), container.end(), container.begin(), ::tolower);

		MenuItem_t item;
		item.type = MENUITEM_SLIDERINT;
		item.script = extract_owner(s);
		item.label = label;
		item.key = key;
		item.i_default = def.value_or(0);
		item.i_min = min;
		item.i_max = max;
		item.format = format.value_or("%d");
		item.callback = cb.value_or(sol::nil);

		g_lua.menu_items[tab][container].push_back(item);
		return key;
	}

	std::string new_slider_float(sol::this_state s, std::string tab, std::string container, std::string label, std::string key, float min, float max, std::optional<std::string> format, std::optional<float> def, std::optional<sol::function> cb) {
		std::transform(tab.begin(), tab.end(), tab.begin(), ::tolower);
		std::transform(container.begin(), container.end(), container.begin(), ::tolower);

		MenuItem_t item;
		item.type = MENUITEM_SLIDERFLOAT;
		item.script = extract_owner(s);
		item.label = label;
		item.key = key;
		item.f_default = def.value_or(0.f);
		item.f_min = min;
		item.f_max = max;
		item.format = format.value_or("%.0f");
		item.callback = cb.value_or(sol::nil);

		g_lua.menu_items[tab][container].push_back(item);
		return key;
	}

	std::string new_keybind(sol::this_state s, std::string tab, std::string container, std::string id, std::string key, std::optional<bool> allow_sc, std::optional<int> def, std::optional<sol::function> cb) {
		std::transform(tab.begin(), tab.end(), tab.begin(), ::tolower);
		std::transform(container.begin(), container.end(), container.begin(), ::tolower);

		MenuItem_t item;
		item.type = MENUITEM_KEYBIND;
		item.script = extract_owner(s);
		item.label = id;
		item.key = key;
		item.allow_style_change = allow_sc.value_or(true);
		item.i_default = def.value_or(0);
		item.callback = cb.value_or(sol::nil);

		g_lua.menu_items[tab][container].push_back(item);
		return key;
	}

	std::string new_text(sol::this_state s, std::string tab, std::string container, std::string label, std::string key) {
		std::transform(tab.begin(), tab.end(), tab.begin(), ::tolower);
		std::transform(container.begin(), container.end(), container.begin(), ::tolower);

		MenuItem_t item;
		item.type = MENUITEM_TEXT;
		item.script = extract_owner(s);
		item.label = label;
		item.key = key;

		g_lua.menu_items[tab][container].push_back(item);
		return key;
	}

	std::string new_singleselect(sol::this_state s, std::string tab, std::string container, std::string label, std::string key, std::vector<const char*> items, std::optional<int> def, std::optional<sol::function> cb) {
		std::transform(tab.begin(), tab.end(), tab.begin(), ::tolower);
		std::transform(container.begin(), container.end(), container.begin(), ::tolower);

		MenuItem_t item;
		item.type = MENUITEM_SINGLESELECT;
		item.script = extract_owner(s);
		item.label = label;
		item.key = key;
		item.i_default = def.value_or(0);
		item.items = items;
		item.callback = cb.value_or(sol::nil);

		g_lua.menu_items[tab][container].push_back(item);
		return key;
	}

	std::string new_multiselect(sol::this_state s, std::string tab, std::string container, std::string label, std::string key, std::vector<const char*> items, std::optional<std::map<int, bool>> def, std::optional<sol::function> cb) {
		std::transform(tab.begin(), tab.end(), tab.begin(), ::tolower);
		std::transform(container.begin(), container.end(), container.begin(), ::tolower);

		MenuItem_t item;
		item.type = MENUITEM_MULTISELECT;
		item.script = extract_owner(s);
		item.label = label;
		item.key = key;
		item.m_default = def.value_or(std::map<int, bool> {});
		item.items = items;
		item.callback = cb.value_or(sol::nil);

		g_lua.menu_items[tab][container].push_back(item);
		return key;
	}

	std::string new_colorpicker(sol::this_state s, std::string tab, std::string container, std::string id, std::string key, std::optional<int> r, std::optional<int> g, std::optional<int> b, std::optional<int> a, std::optional<sol::function> cb) {
		std::transform(tab.begin(), tab.end(), tab.begin(), ::tolower);
		std::transform(container.begin(), container.end(), container.begin(), ::tolower);

		MenuItem_t item;
		item.type = MENUITEM_COLORPICKER;
		item.script = extract_owner(s);
		item.label = id;
		item.key = key;
		item.c_default[0] = r.value_or(255) / 255.f;
		item.c_default[1] = g.value_or(255) / 255.f;
		item.c_default[2] = b.value_or(255) / 255.f;
		item.c_default[3] = a.value_or(255) / 255.f;
		item.callback = cb.value_or(sol::nil);

		g_lua.menu_items[tab][container].push_back(item);
		return key;
	}

	std::string new_button(sol::this_state s, std::string tab, std::string container, std::string id, std::string key, std::optional<sol::function> cb) {
		std::transform(tab.begin(), tab.end(), tab.begin(), ::tolower);
		std::transform(container.begin(), container.end(), container.begin(), ::tolower);

		MenuItem_t item;
		item.type = MENUITEM_BUTTON;
		item.script = extract_owner(s);
		item.label = id;
		item.key = key;
		item.callback = cb.value_or(sol::nil);

		g_lua.menu_items[tab][container].push_back(item);
		return key;
	}

	void set_visibility(std::string key, bool v) {
		for (auto t : g_lua.menu_items) {
			for (auto c : t.second) {
				for (auto& i : c.second) {
					if (i.key == key)
						i.is_visible = v;
				}
			}
		}
	}

	void set_items(std::string key, std::vector<const char*> items) {
		for (auto t : g_lua.menu_items) {
			for (auto c : t.second) {
				for (auto& i : c.second) {
					if (i.key == key)
						i.items = items;
				}
			}
		}
	}

	void set_callback(std::string key, sol::function v) {
		for (auto t : g_lua.menu_items) {
			for (auto c : t.second) {
				for (auto& i : c.second) {
					if (i.key == key)
						i.callback = v;
				}
			}
		}
	}

	void set_label(std::string key, std::string v) {
		for (auto t : g_lua.menu_items) {
			for (auto c : t.second) {
				for (auto& i : c.second) {
					if (i.key == key)
						i.label = v;
				}
			}
		}
	}

	bool is_bind_active(std::string key) {
		return g_hooks.auto_check(key);
	}
	bool get_visible()
	{
		return g_hooks.Is_Open;
	}

	void set_visible(bool visible)
	{
		g_hooks.Is_Open = visible;
	}
	void is_key_down(int key)
	{
		g_hooks.is_key_down(key);
	}
}

namespace ns_surface {
	void set_texture(int id) {
		g_csgo.m_surface->DrawSetTexture(id);
	}

	void set_texture_rgba(int id, const unsigned char* rgba, int w, int h) {
		g_csgo.m_surface->DrawSetTextureRGBA(id, rgba, w, h);
	}

	void set_color(int r, int g, int b, int a) {
		g_csgo.m_surface->DrawSetColor(Color(r, g, b, a));
	}

	int create_texture() {
		return g_csgo.m_surface->CreateNewTextureID();
	}

	void draw_filled_rect(int x, int y, int x2, int y2) {
		g_csgo.m_surface->DrawFilledRect(x, y, x2, y2);
	}

	void draw_outlined_rect(int x, int y, int x2, int y2) {
		g_csgo.m_surface->DrawOutlinedRect(x, y, x2, y2);
	}

	std::tuple<int, int> get_text_size(int font, std::wstring text) {
		int w, h;
		g_csgo.m_surface->GetTextSize(font, text.c_str(), w, h);
		return std::make_tuple(w, h);
	}

	void draw_line(int x, int y, int x2, int y2) {
		g_csgo.m_surface->DrawLine(x, y, x2, y2);
	}

	void draw_outlined_circle(int x, int y, int radius, int segments) {
		g_csgo.m_surface->DrawOutlinedCircle(x, y, radius, segments);
	}

	int create_font(std::string fontname, int w, int h, int blur, int flags) {
		auto f = g_csgo.m_surface->CreateFont();
		g_csgo.m_surface->SetFontGlyphSet(f, fontname.c_str(), w, h, blur, 0, flags);
		return f;
	}

	void set_text_font(int font) {
		g_csgo.m_surface->DrawSetTextFont(font);
	}

	void set_text_color(int r, int g, int b, int a) {
		g_csgo.m_surface->DrawSetTextColor(Color(r, g, b, a));
	}

	void set_text_pos(int x, int y) {
		g_csgo.m_surface->DrawSetTextPos(x, y);
	}

	void draw_text(std::wstring str) {
		g_csgo.m_surface->DrawPrintText(str.c_str(), str.length());
	}

	void draw_textured_rect(int x, int y, int x2, int y2) {
		g_csgo.m_surface->DrawTexturedRect(x, y, x2, y2);
	}


	void draw_filled_rect_fade(int x, int y, int x2, int y2, int alpha, int alpha2, bool horizontal) {
		g_csgo.m_surface->DrawFilledRectFade(x, y, x2, y2, alpha, alpha2, horizontal);
	}
	void draw_string(int x, int y, int r, int g, int b,int a, const std::string& text)
	{

		render::menu.string(x, y, Color(r,g,b,a), text.c_str(), render::ALIGN_RIGHT);

	}
	void DrawColoredCircle(int x, int y,float radius, int r, int g, int b, int a)
	{

		g_csgo.m_surface->DrawColoredCircle(x, y, radius, r, g, b, a);

	}
	void DrawTexturedPolygon(int count, Vertex* verts, bool unk = false)
	{

		g_csgo.m_surface->DrawTexturedPolygon(count,verts,unk);

	}
}

// ----- lua functions -----

c_lua g_lua;
#pragma optimize( "", off ) //видал кака€ хйун€ естьхахаха

template<class T>
T propGetter(int entityID, std::string tableName, std::string propName)
{
	Player* player = g_csgo.m_entlist->GetClientEntity< Player* >(entityID);
	if (!player || !player->IsPlayer())
		return T{};

	auto offset = g_netvars.get(hash32_t{ FNV1a::get(tableName) }, hash32_t{ FNV1a::get(propName) });

	if(!offset)
		return T{};
 
	return player->get<T>(offset);
}

size_t pPropGetter(int entityID, std::string tableName, std::string propName)
{
	Player* player = g_csgo.m_entlist->GetClientEntity< Player* >(entityID);
	if (!player || !player->IsPlayer())
		return size_t{};

	auto offset = g_netvars.get(hash32_t{ FNV1a::get(tableName) }, hash32_t{ FNV1a::get(propName) });

	return offset;
}

struct luaProp
{
	void* propptr;
	luaProp() {
		return;
	}
	luaProp(void* pNetvar) {
		propptr = pNetvar;
	}
	int GetInt()
	{
		if (!propptr)
			return {};

		return *(int*)(propptr);
	}
};

struct luaPlayer
{
	int entityID;
	luaPlayer()
	{
		return;
	}
	luaPlayer(int id)
	{
		entityID = id;
	}
	int getHealth()
	{
		Player* player = g_csgo.m_entlist->GetClientEntity< Player* >(entityID);
		if (!player || !player->IsPlayer())
			return 0;
		return player->m_iHealth();
	}
	luaProp getProp(std::string tableName, std::string propName)
	{
		Player* player = g_csgo.m_entlist->GetClientEntity< Player* >(entityID);
		if (!player || !player->IsPlayer())
			return {};
		auto offset = pPropGetter(entityID, tableName, propName);
		if (!offset)
			return {};
		luaProp* netvar = new luaProp((void*)(player + offset));
		return *netvar;
	}
};


int getIntValue(std::string name)
{
	if (g_hooks.i.count(name) == 0)
	{
		std::string name2 = name + "_lua";
		if (g_hooks.i.count(name2) != 0)
			return g_hooks.i[name2];
		return {};
	}

	return g_hooks.i[name];
}

bool getBoolValue(std::string name)
{
	if (g_hooks.b.count(name) == 0)
	{
		std::string name2 = name + "_lua";
		if (g_hooks.b.count(name2) != 0)
			return g_hooks.b[name2];
		return {};
	}

	return g_hooks.b[name];
}
float getFloatValue(std::string name)
{
	if (g_hooks.f.count(name) == 0)
	{
		std::string name2 = name + "_lua";
		if (g_hooks.f.count(name2) != 0)
			return g_hooks.f[name2];
		return {};
	}

	return g_hooks.f[name];
}

std::vector<bool> getMulti(std::string name)
{
	std::vector<bool> vRet = {};
	if (g_hooks.m.count(name) == 0)
		return vRet;

	auto storedArr = g_hooks.m[name];

	for (int i = 0; i < storedArr.size(); i++)
	{
		vRet.push_back(storedArr[i]);
	}

	return vRet;

}

void setIntValue(std::string name, int value)
{
	if (g_hooks.i.count(name) == 0)
	{
		std::string name2 = name + "_lua";
		if (g_hooks.i.count(name2) != 0)
			g_hooks.i[name2] = value;
		return;
	}

	g_hooks.i[name] = value;
}
void setBoolValue(std::string name, bool value)
{
	if (g_hooks.b.count(name) == 0)
	{
		std::string name2 = name + "_lua";
		if (g_hooks.b.count(name2) != 0)
			g_hooks.b[name2] = value;
		return;
	}

	g_hooks.b[name] = value;
}
void setFloatValue(std::string name, float value)
{
	if (g_hooks.f.count(name) == 0)
	{
		std::string name2 = name + "_lua";
		if (g_hooks.f.count(name2) != 0)
			g_hooks.f[name2] = value;
		return;
	}

	g_hooks.f[name] = value;
}

void cheatprintfs(std::string s)
{
	g_notify.add(s.c_str());
}

void cheatprintfb(bool s)
{
	g_notify.add(s == true ? XOR("true") : XOR("false"));
}

void cheatprintfvec(vec3_t s)
{
	char buff[128];
	sprintf(buff, XOR("x: %.3f y: %.3f z: %.3f"), s.x, s.y, s.z);
	g_notify.add(buff);
}

void cheatprintfvec2(ang_t s)
{
	char buff[128];
	sprintf(buff, XOR("x: %.3f y: %.3f z: %.3f"), s.x, s.y, s.z);
	g_notify.add(buff);
}


void cheatprintfi(int s)
{
	char buff[64];
	sprintf(buff, XOR("%d"), s);
	g_notify.add(buff);
}

void cheatprintff(float s)
{
	char buff[64];
	sprintf(buff, XOR("%.3f"), s);
	g_notify.add(buff);
}

int c_lua::extract_owner(sol::this_state st) {
	sol::state_view lua_state(st);
	sol::table rs = lua_state["debug"]["getinfo"](2, "S");
	std::string source = rs["source"];
	std::string filename = source.substr(1);
	int ret = -1;
	for (int i = 0; i < this->pathes.size(); i++)
	{
		if (this->pathes.at(i) == filename)
			return i;
	}
	return ret;
}

bool is_key_down(int key) {
	return HIWORD(GetKeyState(key));
}
bool is_key_up(int key) {
	return !HIWORD(GetKeyState(key));
}

bool is_key_pressed(int key) {
	return false;
}


bool bindCheck(std::string key)
{
	switch (g_hooks.i[key + XOR("style")]) {
	case 0:
		return true;
	case 1:
		return is_key_down(g_hooks.i[key]);
	case 2:
		return LOWORD(GetKeyState(g_hooks.i[key]));
	case 3:
		return is_key_up(g_hooks.i[key]);
	default:
		return true;
	}

}


void c_lua::init() {
	this->lua = sol::state(sol::c_call<decltype(&lua_panic), &lua_panic>);
	this->lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table,sol::lib::bit32, sol::lib::math, sol::lib::debug,sol::lib::package);

	this->lua[XOR("collectgarbage")] = sol::nil;
	this->lua[XOR("dofile")] = sol::nil;
	this->lua[XOR("load")] = sol::nil;
	this->lua[XOR("loadfile")] = sol::nil;
	this->lua[XOR("pcall")] = sol::nil;
	this->lua[XOR("print")] = sol::nil;
	this->lua[XOR("xpcall")] = sol::nil;
	this->lua[XOR("getmetatable")] = sol::nil;
	this->lua[XOR("setmetatable")] = sol::nil;
	this->lua[XOR("__nil_callback")] = []() {};

	this->lua[XOR("print")] = sol::overload(&cheatprintfs, &cheatprintfb, &cheatprintfvec, &cheatprintfi, &cheatprintff, &cheatprintfvec2);
	this->lua[XOR("error")] = [](std::string s) { g_notify.add(s.c_str()); };

	this->lua.new_usertype<luaPlayer>(XOR("c_player"),
		sol::constructors<luaPlayer(), luaPlayer(int)>(),
		XOR("get_health"), &luaPlayer::getHealth,
		XOR("get_prop"), &luaPlayer::getProp
		);

	this->lua.new_usertype<luaProp>(XOR("c_prop"),
		XOR("get_int"), &luaProp::GetInt
		);

	sol::table lua_cfgClass = this->lua["cfgMgr"].get_or_create<sol::table>();
	//auto bark = lua["bark"].get_or_create<sol::table>();

	lua_cfgClass.set_function("setInt", &setIntValue);
	lua_cfgClass.set_function("setBool", &setBoolValue);
	lua_cfgClass.set_function("setFloat", &setFloatValue);
	lua_cfgClass.set_function("getInt", &getIntValue);
	lua_cfgClass.set_function("getFloat", &getFloatValue);
	lua_cfgClass.set_function("getBool", &getBoolValue);
	lua_cfgClass.set_function("isKeyActive", &bindCheck);
	lua_cfgClass.set_function("isKeyDown", &is_key_down);

	//lua_cfgClass.new_usertype<luaConf>("peek",  
	//	"setInt", &luaConf::setIntValue,
	//	"setBool", &luaConf::setBoolValue,
	//	"setFloat", &luaConf::setFloatValue); // the usual

	this->lua.new_usertype<WeaponInfo>(XOR("c_weaponinfo"),
		XOR("ammo_type"), sol::readonly(&WeaponInfo::m_ammo_type),
		XOR("armor_ratio"), sol::readonly(&WeaponInfo::m_armor_ratio),
		XOR("bullets"), sol::readonly(&WeaponInfo::m_bullets),
		XOR("cycletime"), sol::readonly(&WeaponInfo::m_cycletime),
		XOR("cycletime_alt"), sol::readonly(&WeaponInfo::m_cycletime_alt),
		XOR("damage"), sol::readonly(&WeaponInfo::m_damage),
		XOR("current_clip1"), sol::readonly(&WeaponInfo::m_default_clip1),
		XOR("current_clip2"), sol::readonly(&WeaponInfo::m_default_clip2),
		XOR("max_clip1"), sol::readonly(&WeaponInfo::m_max_clip1),
		XOR("max_clip2"), sol::readonly(&WeaponInfo::m_max_clip2),
		XOR("max_player_speed"), sol::readonly(&WeaponInfo::m_max_player_speed),
		XOR("penetration"), sol::readonly(&WeaponInfo::m_penetration),
		XOR("range"), sol::readonly(&WeaponInfo::m_range),
		XOR("range_modifier"), sol::readonly(&WeaponInfo::m_range_modifier),
		XOR("spread"), sol::readonly(&WeaponInfo::m_spread)
		);

	this->lua.new_usertype<CUserCmd>(XOR("c_usercmd"),
		XOR("command_number"), sol::readonly(&CUserCmd::m_command_number),
		XOR("tick_count"), sol::readonly(&CUserCmd::m_tick),
		XOR("viewangles"), &CUserCmd::m_view_angles,
		XOR("aimdirection"), &CUserCmd::m_aimdirection,
		XOR("forwardmove"), &CUserCmd::m_forward_move,
		XOR("sidemove"), &CUserCmd::m_side_move,
		XOR("upmove"), &CUserCmd::m_up_move,
		XOR("buttons"), &CUserCmd::m_buttons,
		XOR("impulse"), sol::readonly(&CUserCmd::m_impulse),
		XOR("weaponselect"), &CUserCmd::m_weapon_select,
		XOR("weaponsubtype"), sol::readonly(&CUserCmd::m_weapon_subtype),
		XOR("random_seed"), sol::readonly(&CUserCmd::m_random_seed),
		XOR("mousedx"), &CUserCmd::m_mousedx,
		XOR("mousedy"), &CUserCmd::m_mousedy,
		XOR("hasbeenpredicted"), sol::readonly(&CUserCmd::m_predicted)
		);
	this->lua.new_usertype<IGameEvent>(XOR("c_gameevent"),
		XOR("get_name"), &IGameEvent::GetName,
		XOR("is_reliable"), &IGameEvent::IsReliable,
		XOR("is_local"), &IGameEvent::IsLocal,
		XOR("is_empty"), &IGameEvent::IsEmpty,
		XOR("get_bool"), &IGameEvent::GetBool,
		XOR("get_int"), &IGameEvent::GetInt,
		XOR("get_uint64"), &IGameEvent::GetUint64,
		XOR("get_float"), &IGameEvent::GetFloat,
		XOR("get_string"), &IGameEvent::GetString
		);
	this->lua.new_enum(XOR("HITBOXES"),
		XOR("HEAD"), Hitboxes_t::HITBOX_HEAD,
		XOR("NECK"), Hitboxes_t::HITBOX_NECK,
		XOR("PELVIS"), Hitboxes_t::HITBOX_PELVIS,
		XOR("BODY"), Hitboxes_t::HITBOX_BODY,
		XOR("THORAX"), Hitboxes_t::HITBOX_THORAX,
		XOR("CHEST"), Hitboxes_t::HITBOX_CHEST,
		XOR("UPPER_CHEST"), Hitboxes_t::HITBOX_UPPER_CHEST,
		XOR("RIGHT_THIGH"), Hitboxes_t::HITBOX_R_THIGH,
		XOR("LEFT_THIGH"), Hitboxes_t::HITBOX_L_THIGH,
		XOR("RIGHT_CALF"), Hitboxes_t::HITBOX_R_CALF,
		XOR("LEFT_CALF"), Hitboxes_t::HITBOX_L_CALF,
		XOR("RIGHT_FOOT"), Hitboxes_t::HITBOX_R_FOOT,
		XOR("LEFT_FOOT"), Hitboxes_t::HITBOX_L_FOOT,
		XOR("RIGHT_HAND"), Hitboxes_t::HITBOX_R_HAND,
		XOR("LEFT_HAND"), Hitboxes_t::HITBOX_L_HAND,
		XOR("RIGHT_UPPER_ARM"), Hitboxes_t::HITBOX_R_UPPER_ARM,
		XOR("RIGHT_FOREARM"), Hitboxes_t::HITBOX_R_FOREARM,
		XOR("LEFT_UPPER_ARM"), Hitboxes_t::HITBOX_L_UPPER_ARM,
		XOR("LEFT_FOREARM"), Hitboxes_t::HITBOX_L_FOREARM
	);
	this->lua.new_enum(XOR("FONTFLAGS"),
		XOR("NONE"), EFontFlags::FONTFLAG_NONE,
		XOR("ITALIC"), EFontFlags::FONTFLAG_ITALIC,
		XOR("UNDERLINE"), EFontFlags::FONTFLAG_UNDERLINE,
		XOR("STRIKEOUT"), EFontFlags::FONTFLAG_STRIKEOUT,
		XOR("SYMBOL"), EFontFlags::FONTFLAG_SYMBOL,
		XOR("ANTIALIAS"), EFontFlags::FONTFLAG_ANTIALIAS,
		XOR("GAUSSIANBLUR"), EFontFlags::FONTFLAG_GAUSSIANBLUR,
		XOR("ROTARY"), EFontFlags::FONTFLAG_ROTARY,
		XOR("DROPSHADOW"), EFontFlags::FONTFLAG_DROPSHADOW,
		XOR("ADDITIVE"), EFontFlags::FONTFLAG_ADDITIVE,
		XOR("OUTLINE"), EFontFlags::FONTFLAG_OUTLINE,
		XOR("CUSTOM"), EFontFlags::FONTFLAG_CUSTOM,
		XOR("BITMAP"), EFontFlags::FONTFLAG_BITMAP
	);

	this->lua.new_usertype<BeamInfo_t>(XOR("c_beaminfo"),
		sol::constructors<BeamInfo_t()>(),
		XOR("type"), &BeamInfo_t::m_nType,
		XOR("start_ent"), &BeamInfo_t::m_pStartEnt,
		XOR("start_attachment"), &BeamInfo_t::m_nStartAttachment,
		XOR("end_ent"), &BeamInfo_t::m_pEndEnt,
		XOR("end_attachment"), &BeamInfo_t::m_nEndAttachment,
		XOR("start"), &BeamInfo_t::m_vecStart,
		XOR("end"), &BeamInfo_t::m_vecEnd,
		XOR("model_index"), &BeamInfo_t::m_nModelIndex,
		XOR("model_name"), &BeamInfo_t::m_pszModelName,
		XOR("halo_index"), &BeamInfo_t::m_nHaloIndex,
		XOR("halo_name"), &BeamInfo_t::m_pszHaloName,
		XOR("halo_scale"), &BeamInfo_t::m_flHaloScale,
		XOR("life"), &BeamInfo_t::m_flLife,
		XOR("width"), &BeamInfo_t::m_flWidth,
		XOR("end_width"), &BeamInfo_t::m_flEndWidth,
		XOR("fade_length"), &BeamInfo_t::m_flFadeLength,
		XOR("amplitude"), &BeamInfo_t::m_flAmplitude,
		XOR("brightness"), &BeamInfo_t::m_flBrightness,
		XOR("speed"), &BeamInfo_t::m_flSpeed,
		XOR("start_frame"), &BeamInfo_t::m_nStartFrame,
		XOR("framerate"), &BeamInfo_t::m_flFrameRate,
		XOR("red"), &BeamInfo_t::m_flRed,
		XOR("green"), &BeamInfo_t::m_flGreen,
		XOR("blue"), &BeamInfo_t::m_flBlue,
		XOR("renderable"), &BeamInfo_t::m_bRenderable,
		XOR("segments"), &BeamInfo_t::m_nSegments,
		XOR("flags"), &BeamInfo_t::m_nFlags,
		XOR("center"), &BeamInfo_t::m_vecCenter,
		XOR("start_radius"), &BeamInfo_t::m_flStartRadius,
		XOR("end_radius"), &BeamInfo_t::m_flEndRadius
		);
	this->lua.new_usertype<Beam_t>(XOR("c_beam"));
	this->lua.new_usertype<player_info_t>(XOR("c_playerinfo"));
	this->lua.new_usertype<ConVar>(XOR("c_convar"));
	this->lua.new_usertype<INetChannel>(XOR("c_netchannelinfo"),
		XOR("get_latency"), &INetChannel::GetLatency,
		XOR("get_avg_latency"), &INetChannel::GetAvgLatency
		);
	this->lua.new_usertype<INetChannel>(XOR("c_netchannel"),
		XOR("out_sequence_nr"), sol::readonly(&INetChannel::m_out_seq),
		XOR("in_sequence_nr"), sol::readonly(&INetChannel::m_in_seq),
		XOR("out_sequence_nr_ack"), sol::readonly(&INetChannel::m_out_seq_ack),
		XOR("out_reliable_state"), sol::readonly(&INetChannel::m_out_rel_state),
		XOR("in_reliable_state"), sol::readonly(&INetChannel::m_in_rel_state),
		XOR("choked_packets"), sol::readonly(&INetChannel::m_choked_packets)
		);
	this->lua.new_usertype<Color>(XOR("c_color"),
		sol::constructors<Color(), Color(int, int, int), Color(int, int, int, int)>(),
		XOR("r"), &Color::r,
		XOR("g"), &Color::g,
		XOR("b"), &Color::b,
		XOR("a"), &Color::a
		);
	this->lua.new_usertype<vec2_t>(XOR("c_vec2_t"),
		sol::constructors<vec2_t(), vec2_t(float, float), vec2_t(vec2_t)>(),
		XOR("x"), &vec2_t::x,
		XOR("y"), &vec2_t::y,
		XOR("length"), &vec2_t::length
		);
	this->lua.new_usertype<Vertex>(XOR("c_vertex"),
		sol::constructors<Vertex(), Vertex(vec2_t), Vertex(vec2_t, vec2_t)>(),
		XOR("init"), &Vertex::init,
		XOR("position"), &Vertex::m_pos,
		XOR("tex_coord"), &Vertex::m_coord
		);
	this->lua.new_usertype<ang_t>(XOR("c_ang_t"),
		sol::constructors<ang_t(), ang_t(float, float, float)>(),
		XOR("x"), &ang_t::x,
		XOR("y"), &ang_t::y,
		XOR("z"), &ang_t::z,
		XOR("normalize"), &ang_t::normalize
		);
	this->lua.new_usertype<vec3_t>(XOR("c_vec3_t"),
		sol::constructors<vec3_t(), vec3_t(float, float, float)>(),
		XOR("x"), &vec3_t::x,
		XOR("y"), &vec3_t::y,
		XOR("z"), &vec3_t::z,
		XOR("length"), &vec3_t::length,
		XOR("length_sqr"), &vec3_t::length_sqr,
		XOR("length_2d"), &vec3_t::length_2d,
		XOR("length_2d_sqr"), &vec3_t::length_2d_sqr,
		XOR("dist_to"), &vec3_t::dist_to,
		XOR("cross_product"), &vec3_t::cross,
		XOR("normalize"), &vec3_t::normalize,
		XOR("normalize_angles"), &vec3_t::normalized
		);
	this->lua.new_usertype<studiohdr_t>(XOR("c_studiohdr"),
		XOR("id"), sol::readonly(&studiohdr_t::m_id),
		XOR("version"), sol::readonly(&studiohdr_t::m_version),
		XOR("checksum"), sol::readonly(&studiohdr_t::m_checksum),
		XOR("length"), sol::readonly(&studiohdr_t::m_length),
		XOR("eyeposition"), sol::readonly(&studiohdr_t::m_eye_pos),
		XOR("illumposition"), sol::readonly(&studiohdr_t::m_illum_pos),
		XOR("hull_min"), sol::readonly(&studiohdr_t::m_hull_mins),
		XOR("hull_max"), sol::readonly(&studiohdr_t::m_hull_maxs),
		XOR("view_bbmin"), sol::readonly(&studiohdr_t::m_view_mins),
		XOR("view_bbmax"), sol::readonly(&studiohdr_t::m_view_maxs),
		XOR("flags"), sol::readonly(&studiohdr_t::m_flags),
		XOR("numbones"), sol::readonly(&studiohdr_t::m_num_bones),
		XOR("get_bone"), &studiohdr_t::GetBone,
		XOR("numhitboxsets"), sol::readonly(&studiohdr_t::m_num_sets),
		XOR("get_hitbox_set"), &studiohdr_t::GetHitboxSet
		);
	this->lua.new_usertype<mstudiohitboxset_t>(XOR("c_studiohitboxset"),
		XOR("get_hitbox"), &mstudiohitboxset_t::GetHitbox
		);
	this->lua.new_usertype<mstudiobbox_t>(XOR("c_studiobbox"),
		XOR("bone"), sol::readonly(&mstudiobbox_t::m_bone),
		XOR("group"), sol::readonly(&mstudiobbox_t::m_group),
		XOR("bbmin"), sol::readonly(&mstudiobbox_t::m_mins),
		XOR("bbmax"), sol::readonly(&mstudiobbox_t::m_maxs),
		XOR("radius"), sol::readonly(&mstudiobbox_t::m_radius),
		XOR("get_hitbox_name_id"), &mstudiobbox_t::m_name_id
		);
	this->lua.new_usertype<model_t>(XOR("c_model"));
	this->lua.new_usertype<CCSGOPlayerAnimState>(XOR("c_animstate"),
		XOR("entity"), &CCSGOPlayerAnimState::m_player,

		XOR("yaw"), &CCSGOPlayerAnimState::m_eye_yaw,
		XOR("pitch"), &CCSGOPlayerAnimState::m_eye_pitch
		);
	this->lua.new_usertype<Player>(XOR("c_baseentity"),
		XOR("is_player"), &Player::IsPlayer,
		XOR("get_abs_origin"), &Player::GetAbsOrigin,
		XOR("get_index"), &Player::index,
		XOR("get_spawn_time"), &Player::m_flSpawnTime,
		XOR("get_anim_state"), &Player::m_PlayerAnimState
		);
	this->lua.new_usertype<ModelRenderInfo_t>(XOR("c_modelrenderinfo"),
		XOR("origin"), sol::readonly(&ModelRenderInfo_t::m_origin),
		XOR("angles"), sol::readonly(&ModelRenderInfo_t::m_angles),
		XOR("entity_index"), sol::readonly(&ModelRenderInfo_t::m_index)
		);
	this->lua.new_usertype<CViewSetup>(XOR("c_viewsetup"),
		XOR("x"), &CViewSetup::m_x,
		XOR("x_old"), &CViewSetup::m_old_x,
		XOR("y"), &CViewSetup::m_y,
		XOR("y_old"), &CViewSetup::m_old_y,
		XOR("width"), &CViewSetup::m_width,
		XOR("width_old"), &CViewSetup::m_old_width,
		XOR("height"), &CViewSetup::m_height,
		XOR("height_old"), &CViewSetup::m_old_height,
		XOR("fov"), &CViewSetup::m_fov,
		XOR("viewmodel_fov"), &CViewSetup::m_viewmodel_fov,
		XOR("origin"), &CViewSetup::m_origin,
		XOR("angles"), &CViewSetup::m_angles
		);

	auto config = this->lua.create_table();
	config[XOR("get")] = ns_config::get;
	config[XOR("set")] = sol::overload(ns_config::set_bool, ns_config::set_color, ns_config::set_float, ns_config::set_multiselect, ns_config::set_int);
	config[XOR("load")] = ns_config::load;
	config[XOR("save")] = ns_config::save;

	auto cheat = this->lua.create_table();
	cheat[XOR("set_event_callback")] = ns_cheat::set_event_callback;
	cheat[XOR("run_script")] = ns_cheat::run_script;
	cheat[XOR("reload_active_scripts")] = ns_cheat::reload_active_scripts;

	auto surface = this->lua.create_table();
	surface[XOR("create_font")] = ns_surface::create_font;
	surface[XOR("create_texture")] = ns_surface::create_texture;
	surface[XOR("draw_filled_rect")] = ns_surface::draw_filled_rect;
	surface[XOR("draw_line")] = ns_surface::draw_line;
	surface[XOR("draw_outlined_circle")] = ns_surface::draw_outlined_circle;
	surface[XOR("draw_outlined_rect")] = ns_surface::draw_outlined_rect;
	surface[XOR("draw_text")] = ns_surface::draw_text;
	surface[XOR("draw_string")] = ns_surface::draw_string;
	surface[XOR("draw_textured_rect")] = ns_surface::draw_textured_rect;
	surface[XOR("get_text_size")] = ns_surface::get_text_size;
	//surfaceXOR(["indicator"] = ns_surface::indicator;
	surface[XOR("set_color")] = ns_surface::set_color;
	surface[XOR("set_texture")] = sol::overload(ns_surface::set_texture, ns_surface::set_texture_rgba);
	surface[XOR("set_text_color")] = ns_surface::set_text_color;
	surface[XOR("set_text_font")] = ns_surface::set_text_font;
	surface[XOR("set_text_pos")] = ns_surface::set_text_pos;
	surface[XOR("draw_filled_rect_fade")] = ns_surface::draw_filled_rect_fade;

	auto models = this->lua.create_table();
	models[XOR("get_model_index")] = ns_models::get_model_index;
	models[XOR("get_studio_model")] = ns_models::get_studio_model;

	auto engine = this->lua.create_table();
	engine[XOR("client_cmd")] = ns_engine::client_cmd;
	engine[XOR("execute_client_cmd")] = ns_engine::execute_client_cmd;
	engine[XOR("get_last_timestamp")] = ns_engine::get_last_timestamp;
	engine[XOR("get_local_player_index")] = ns_engine::get_local_player_index;
	engine[XOR("get_max_clients")] = ns_engine::get_max_clients;
	engine[XOR("get_net_channel_info")] = ns_engine::get_net_channel_info;
	engine[XOR("get_player_for_user_id")] = ns_engine::get_player_for_user_id;
	engine[XOR("get_player_info")] = ns_engine::get_player_info;
	engine[XOR("get_screen_size")] = ns_engine::get_screen_size;
	engine[XOR("get_view_angles")] = ns_engine::get_view_angles;
	engine[XOR("is_in_game")] = ns_engine::is_in_game;
	engine[XOR("is_paused")] = ns_engine::is_paused;
	engine[XOR("set_view_angles")] = ns_engine::set_view_angles;
	engine[XOR("username")] = ns_engine::username;
	engine[XOR("getping")] = ns_engine::getping1;
	engine[XOR("get_prop_int")], & Player::dormant;
	engine[XOR("get_prop_float")], & Player::GetPropFloat;
	engine[XOR("get_prop_bool")], & Player::GetPropBool;
	engine[XOR("set_prop_int")], & Player::SetPropInt;
	engine[XOR("set_prop_float")], & Player::SetPropFloat;
	engine[XOR("set_prop_bool")], & Player::SetPropBool;
	auto entity_list = this->lua.create_table();
	entity_list[XOR("get_client_entity")] = ns_entity_list::get_client_entity;
	entity_list[XOR("get_client_entity_from_handle")] = ns_entity_list::get_client_entity_from_handle;
	entity_list[XOR("get_highest_entity_index")] = ns_entity_list::get_highest_entity_index;
	entity_list[XOR("get_local_player")] = ns_entity_list::get_local_player;

	auto utils = this->lua.create_table();
	utils[XOR("get_player_data")] = ns_utils::get_player_data;
	utils[XOR("clamp")] = ns_utils::clamp;
	utils[XOR("world_to_screen")] = ns_utils::world_to_screen;

	auto globals = this->lua.create_table();
	globals[XOR("realtime")] = ns_globals::realtime;
	globals[XOR("framecount")] = ns_globals::framecount;
	globals[XOR("absoluteframetime")] = ns_globals::absoluteframetime;
	globals[XOR("curtime")] = ns_globals::curtime;
	globals[XOR("frametime")] = ns_globals::frametime;
	globals[XOR("maxclients")] = ns_globals::maxclients;
	globals[XOR("tickcount")] = ns_globals::tickcount;
	globals[XOR("tickinterval")] = ns_globals::tickinterval;

	auto trace = this->lua.create_table();
	trace[XOR("get_point_contents")] = ns_trace::get_point_contents;
	//trace["trace_ray"] = ns_trace::trace_ray;

	auto cvar = this->lua.create_table();
	cvar[XOR("console_color_print")] = sol::overload(ns_cvar::console_color_print, ns_cvar::console_color_print_rgba);
	cvar[XOR("console_print")] = ns_cvar::console_print;
	cvar[XOR("find_var")] = ns_cvar::find_var;
	cvar[XOR("get_float")] = ns_cvar::get_float;
	cvar[XOR("get_int")] = ns_cvar::get_int;
	cvar[XOR("set_float")] = ns_cvar::set_float;
	cvar[XOR("set_int")] = ns_cvar::set_int;
	cvar[XOR("get_string")] = ns_cvar::get_string;
	cvar[XOR("set_string")] = ns_cvar::set_string;
	cvar[XOR("unlock_cvar")] = ns_cvar::unlock_cvar;
	cvar[XOR("remove_callbacks")] = ns_cvar::remove_callbacks;

	auto overlay = this->lua.create_table();
	overlay[XOR("add_box_overlay")] = ns_overlay::add_box_overlay;
	overlay[XOR("add_capsule_overlay")] = ns_overlay::add_capsule_overlay;
	overlay[XOR("add_text_overlay")] = ns_overlay::add_text_overlay;

	auto beams = this->lua.create_table();
	beams[XOR("create_points")] = ns_beams::create_points;
	beams[XOR("draw_beam")] = ns_beams::draw_beam;

	auto ui = this->lua.create_table();
	ui[XOR("new_checkbox")] = ns_ui::new_checkbox;
	ui[XOR("new_colorpicker")] = ns_ui::new_colorpicker;
	ui[XOR("new_keybind")] = ns_ui::new_keybind;
	ui[XOR("new_multiselect")] = ns_ui::new_multiselect;
	ui[XOR("new_singleselect")] = ns_ui::new_singleselect;
	ui[XOR("new_slider_float")] = ns_ui::new_slider_float;
	ui[XOR("new_slider_int")] = ns_ui::new_slider_int;
	ui[XOR("new_text")] = ns_ui::new_text;
	ui[XOR("new_button")] = ns_ui::new_button;
	ui[XOR("set_callback")] = ns_ui::set_callback;
	ui[XOR("set_items")] = ns_ui::set_items;
	ui[XOR("set_label")] = ns_ui::set_label;
	ui[XOR("set_visibility")] = ns_ui::set_visibility;
	ui[XOR("is_bind_active")] = ns_ui::is_bind_active;
	ui[XOR("is_key_down")] = ns_ui::is_key_down;

	this->lua[XOR("add_menucombo")] = [](sol::this_state s, std::string name, std::string tag, std::vector < std::string > values, int location) {
		luaMenuItem newItem;

		newItem.name = name;
		tag += "_lua";
		newItem.tag = tag;
		newItem.type = luaMenuItem::luamenuTypes::LUAMENU_COMBO;

		auto got = g_hooks.i.find(tag);

		int scriptID = g_lua.extract_owner(s);

		if (got == g_hooks.i.end())
		{
			//auto queuePos = std::find(g_hooks.luaMenuItems[scriptID].begin(), g_hooks.luaMenuItems[scriptID].end(), tag);
			//if(queuePos == g_hooks.luaMenuItems[scriptID].end())
			g_hooks.i[tag] = 0;
		}

		newItem.value = &g_hooks.i[tag];

		newItem.tabLocation = location;

		newItem.comboitems = values;

		if (scriptID == -1)
			g_notify.add(XOR("SCRIPT PADDING INTERNAL ERROR"));
		else
		{
			g_hooks.luaMenuItems[scriptID].push_back(newItem);
		}

	};

	this->lua[XOR("add_menuslider")] = [](sol::this_state s, std::string name, std::string tag, int minval, int maxval, int location) {
		luaMenuItem newItem;

		newItem.name = name;
		tag += XOR("_lua");
		newItem.tag = tag;
		newItem.type = luaMenuItem::luamenuTypes::LUAMENU_SLIDER;

		auto got = g_hooks.f.find(tag);

		int scriptID = g_lua.extract_owner(s);

		if (got == g_hooks.f.end())
		{
			//auto queuePos = std::find(g_hooks.luaMenuItems[scriptID].begin(), g_hooks.luaMenuItems[scriptID].end(), tag);
			//if(queuePos == g_hooks.luaMenuItems[scriptID].end())
			g_hooks.f[tag] = 0.f;
		}

		newItem.value = &g_hooks.f[tag];

		newItem.tabLocation = location;

		newItem.slidermin = minval;
		newItem.slidermax = maxval;


		if (scriptID == -1)
			g_notify.add(XOR("SCRIPT PADDING INTERNAL ERROR"));
		else
		{
			g_hooks.luaMenuItems[scriptID].push_back(newItem);
		}

	};

	this->lua[XOR("add_menucheckbox")] = [](sol::this_state s, std::string name, std::string tag, int location) {
		luaMenuItem newItem;

		newItem.name = name;
		tag += "_lua";
		newItem.tag = tag;
		newItem.type = luaMenuItem::luamenuTypes::LUAMENU_CHECKBOX;

		auto got = g_hooks.b.find(tag);

		if (got == g_hooks.b.end())
		{
			g_hooks.b[tag] = 0;
		}

		newItem.value = &g_hooks.b[tag];

		newItem.tabLocation = location;

		int scriptID = g_lua.extract_owner(s);

		if (scriptID == -1)
			g_notify.add(XOR("SCRIPT PADDING INTERNAL ERROR"));
		else
		{
			g_hooks.luaMenuItems[scriptID].push_back(newItem);
		}


	};


	this->lua[XOR("set_bool")] = [](std::string s, bool value) {
		if (itemMap.find(s) == itemMap.end())
		{
			return;
		}
		bool* ret = (bool*)itemMap[s].huesos;
		if (ret) { *ret = value; } };

	this->lua[XOR("set_bool")] = [](std::string s, bool value) {
		if (itemMap.find(s) == itemMap.end())
		{
			return;
		}
		bool* ret = (bool*)itemMap[s].huesos;
		if (ret) { *ret = value; } };

	this->lua[XOR("set_slider")] = [](std::string s, float value) {
		if (itemMap.find(s) == itemMap.end())
		{
			return;
		}
		float* ret = (float*)itemMap[s].huesos;
		if (ret) { *ret = value; } };

	this->lua[XOR("get_bool")] = [](std::string s) {
		if (itemMap.find(s) == itemMap.end())
		{
			return false;
		}
		bool* ret = (bool*)itemMap[s].huesos;
		if (ret)  return *ret;
		else return false; };
	this->lua[XOR("get_slider")] = [](std::string s) {
		if (itemMap.find(s) == itemMap.end())
		{
			return 0.f;
		}
		float* ret = (float*)itemMap[s].huesos;
		if (ret)  return *ret;
		else return 0.f; };

	this->lua[XOR("get_combo")] = [](std::string s) {
		if (itemMap.find(s) == itemMap.end()) { return 0; }
		int* pCursor = (int*)itemMap[s].huesos;
		if (!pCursor)
			return 0;
		int cursor = *pCursor;
		return cursor;
	};

	this->lua[XOR("set_combo")] = [](std::string s, int value) {
		if (itemMap.find(s) == itemMap.end()) { return 0; }
		int* pCursor = (int*)itemMap[s].huesos;
		if (!pCursor)
			return 0;
		*pCursor = value;
		return *pCursor;
	};


	this->lua[XOR("get_multicombo")] = [](std::string s) {

	};
	//лоадер потом ебани короче щ€ скачаю те архив вот
	this->lua[XOR("set_multicombo")] = [](std::string s, std::string k, bool value) {
		std::vector<int> vRet = {};
		if (itemMap.find(s) == itemMap.end()) { return vRet; }
		std::string ret = {};

		auto ItemsArray = itemMap[s].itemL;
		auto iter = std::find(begin(ItemsArray), end(ItemsArray), k);
		if (iter == ItemsArray.end()) { return vRet; }
		else
		{
			auto index = std::distance(ItemsArray.begin(), iter);
			auto pidor = itemMap[s].multicomboAss;
			if (!pidor)
				return vRet;

			auto storedArr = (*pidor);

			if (index > storedArr.size())
				return vRet;

			storedArr[index] = value;

		}
		return vRet;

	};


	//this->lua[XOR("get_playerprop")] = [](int entityID, std::string tableName, std::string propName) {
	//	return propGetter<bool>(entityID, tableName, propName);
	//};

	this->lua[XOR("get_playerprop_int")] = [](int entityID, std::string tableName, std::string propName) {
		return propGetter<int>(entityID, tableName, propName);
	};
	this->lua[XOR("get_playerprop_bool")] = [](int entityID, std::string tableName, std::string propName) {
		return propGetter<bool>(entityID, tableName, propName);
	};
	this->lua[XOR("get_playerprop_vec")] = [](int entityID, std::string tableName, std::string propName) {
		return propGetter<vec3_t>(entityID, tableName, propName);
	};
	this->lua[XOR("get_playerprop_float")] = [](int entityID, std::string tableName, std::string propName) {
		return propGetter<float>(entityID, tableName, propName);
	};
	this->lua[XOR("get_player")] = [](int entityID) {
		return luaPlayer(entityID);
	};

	//this->lua[XOR("get_playerprop")] = [](int entityID, std::string tableName, std::string propName) {
	//	return propGetter<float>(entityID, tableName, propName);
	//};

	//this->lua[XOR("get_playerprop")] = [](int entityID, std::string tableName, std::string propName) {
	//	return propGetter<vec3_t>(entityID, tableName, propName);
	//};

	this->lua[XOR("get_playerhealth")] = [](int entityID) {
		Player* player = g_csgo.m_entlist->GetClientEntity< Player* >(entityID);
		if (!player || !player->IsPlayer())
			return 0;
		return player->m_iHealth();
	};

	this->lua[XOR("get_playeralive")] = [](int entityID) {
		Player* player = g_csgo.m_entlist->GetClientEntity< Player* >(entityID);
		if (!player || !player->IsPlayer())
			return false;
		return player->alive();
	};

	this->lua[XOR("get_playerteam")] = [](int entityID) {
		Player* player = g_csgo.m_entlist->GetClientEntity< Player* >(entityID);
		if (!player || !player->IsPlayer())
			return 0;
		return player->m_iTeamNum();
	};

	this->lua[XOR("get_playerorigin")] = [](int entityID) {
		Player* player = g_csgo.m_entlist->GetClientEntity< Player* >(entityID);
		if (!player || !player->IsPlayer())
			return vec3_t{};
		return player->m_vecOrigin();
	};

	this->lua[XOR("get_playerweaponinfo")] = [](int entityID) {
		Player* player = g_csgo.m_entlist->GetClientEntity< Player* >(entityID);
		if (!player || !player->IsPlayer())
			return &WeaponInfo{};
		return player->GetActiveWeapon()->GetWpnData();
	};

	this->lua[XOR("get_playerweapon")] = [](int entityID) {
		Player* player = g_csgo.m_entlist->GetClientEntity< Player* >(entityID);
		if (!player || !player->IsPlayer())
			return 0;
		return player->GetActiveWeapon()->m_iItemDefinitionIndex();
	};



	this->lua[XOR("config")] = config;
	this->lua[XOR("cheat")] = cheat;
	this->lua[XOR("surface")] = surface;
	this->lua[XOR("models")] = models;
	this->lua[XOR("engine")] = engine;
	this->lua[XOR("entity_list")] = entity_list;
	this->lua[XOR("utils")] = utils;
	this->lua[XOR("globals")] = globals;
	this->lua[XOR("trace")] = trace;
	this->lua[XOR("cvar")] = cvar;
	this->lua[XOR("overlay")] = overlay;
	this->lua[XOR("beams")] = beams;
	this->lua[XOR("ui")] = ui;

	this->refresh_scripts();
	//this->load_script(this->get_script_id(XOR("autorun.lua")));
}
#pragma optimize( "", on ) //видал кака€ хйун€ естьхахаха

void c_lua::load_script(int id) {
	if (id == -1)
		return;

	if (this->loaded.at(id))
		return;

	auto path = this->get_script_path(id);
	if (path == (""))
		return;

	this->lua.script_file(path, [](lua_State*, sol::protected_function_result result) {
		if (!result.valid()) {
			sol::error err = result;
			g_notify.add(err.what());
		}

		return result;
		});

	this->loaded.at(id) = true;
}

void c_lua::unload_script(int id) {
	if (id == -1)
		return;

	if (!this->loaded.at(id))
		return;

	std::map<std::string, std::map<std::string, std::vector<MenuItem_t>>> updated_items;
	for (auto i : this->menu_items) {
		for (auto k : i.second) {
			std::vector<MenuItem_t> updated_vec;

			for (auto m : k.second)
				if (m.script != id)
					updated_vec.push_back(m);

			updated_items[k.first][i.first] = updated_vec;
		}
	}
	this->menu_items = updated_items;

	g_hooks.luaMenuItems[id].clear();

	g_lua_hook.unregisterHooks(id);
	this->loaded.at(id) = false;
}

void c_lua::reload_all_scripts() {
	for (auto s : this->scripts) {
		if (this->loaded.at(this->get_script_id(s))) {
			this->unload_script(this->get_script_id(s));
			this->load_script(this->get_script_id(s));
		} 
	}
}

void c_lua::unload_all_scripts() {
	for (auto s : this->scripts)
		if (this->loaded.at(this->get_script_id(s)))
			this->unload_script(this->get_script_id(s));
}

void c_lua::refresh_scripts() {
	auto oldLoaded = this->loaded;
	auto oldScripts = this->scripts;

	this->loaded.clear();
	this->pathes.clear();
	this->scripts.clear();

	for (auto& entry : std::filesystem::directory_iterator("C:\\jre.codes\\lua")) {
		if (entry.path().extension() == (".lua")) {
			auto path = entry.path();
			auto filename = path.filename().string();

			bool didPut = false;
			for (int i = 0; i < oldScripts.size(); i++) {
				if (filename == oldScripts.at(i)) {
					this->loaded.push_back(oldLoaded.at(i));
					didPut = true;
				}
			}

			if (!didPut)
				this->loaded.push_back(false);

			this->pathes.push_back(path);
			this->scripts.push_back(filename);
		}
	}
}

int c_lua::get_script_id(std::string name) {
	for (int i = 0; i < this->scripts.size(); i++) {
		if (this->scripts.at(i) == name)
			return i;
	}

	return -1;
}

int c_lua::get_script_id_by_path(std::string path) {
	for (int i = 0; i < this->pathes.size(); i++) {
		if (this->pathes.at(i).string() == path)
			return i;
	}

	return -1;
}

std::string c_lua::get_script_path(std::string name) {
	return this->get_script_path(this->get_script_id(name));
}

std::string c_lua::get_script_path(int id) {
	if (id == -1)
		return  "";

	return this->pathes.at(id).string();
}
