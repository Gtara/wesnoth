/* $Id$ */
/*
   Copyright (C) 2003 by David White <davidnwhite@optusnet.com.au>
   Part of the Battle for Wesnoth Project http://wesnoth.whitevine.net

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#include "filesystem.hpp"
#include "game_config.hpp"
#include "gamestatus.hpp"
#include "language.hpp"
#include "log.hpp"
#include "statistics.hpp"

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <sstream>

time_of_day::time_of_day(const config& cfg)
                 : lawful_bonus(atoi(cfg["lawful_bonus"].c_str())),
                   image(cfg["image"]), name(cfg["name"]), id(cfg["id"]),
				   image_mask(cfg["mask"]),
                   red(atoi(cfg["red"].c_str())),
                   green(atoi(cfg["green"].c_str())),
                   blue(atoi(cfg["blue"].c_str()))
{
	const std::string& lang_name = string_table[cfg["id"]];
	if(lang_name.empty() == false)
		name = lang_name;
}

void time_of_day::write(config& cfg) const
{
	char buf[50];
	sprintf(buf,"%d",lawful_bonus);
	cfg["lawful_bonus"] = buf;

	sprintf(buf,"%d",red);
	cfg["red"] = buf;

	sprintf(buf,"%d",green);
	cfg["green"] = buf;

	sprintf(buf,"%d",blue);
	cfg["blue"] = buf;

	cfg["image"] = image;
	cfg["name"] = name;
	cfg["id"] = id;
	cfg["mask"] = image_mask;
}

namespace {

void parse_times(const config& cfg, std::vector<time_of_day>& normal_times, std::vector<time_of_day>& illuminated_times)
{
	const config::child_list& times = cfg.get_children("time");
	config::child_list::const_iterator t;
	for(t = times.begin(); t != times.end(); ++t) {
		normal_times.push_back(time_of_day(**t));
	}

	const config::child_list& times_illum = cfg.get_children("illuminated_time");
	const config::child_list& illum = times_illum.empty() ? times : times_illum;

	for(t = illum.begin(); t != illum.end(); ++t) {
		illuminated_times.push_back(time_of_day(**t));
	}
}

}

gamestatus::gamestatus(config& time_cfg, int num_turns) :
                 turn_(1),numTurns_(num_turns)
{
	const std::string& turn_at = time_cfg["turn_at"];
	if(turn_at.empty() == false) {
		turn_ = atoi(turn_at.c_str());
	}

	parse_times(time_cfg,times_,illuminatedTimes_);

	const config::child_list& times_range = time_cfg.get_children("time_area");
	for(config::child_list::const_iterator t = times_range.begin(); t != times_range.end(); ++t) {
		const std::vector<gamemap::location> locs = parse_location_range((**t)["x"],(**t)["y"]);
		area_time_of_day area;
		area.xsrc = (**t)["x"];
		area.ysrc = (**t)["y"];
		std::copy(locs.begin(),locs.end(),std::inserter(area.hexes,area.hexes.end()));
		parse_times(**t,area.times,area.illuminated_times);

		areas_.push_back(area);
	}
}

void gamestatus::write(config& cfg) const
{
	char buf[50];
	sprintf(buf,"%d",turn_);
	cfg["turn_at"] = buf;

	sprintf(buf,"%d",numTurns_);
	cfg["turns"] = buf;

	std::vector<time_of_day>::const_iterator t;
	for(t = times_.begin(); t != times_.end(); ++t) {
		t->write(cfg.add_child("time"));
	}

	for(t = illuminatedTimes_.begin(); t != illuminatedTimes_.end(); ++t) {
		t->write(cfg.add_child("illuminated_time"));
	}

	for(std::vector<area_time_of_day>::const_iterator i = areas_.begin(); i != areas_.end(); ++i) {
		config& area = cfg.add_child("time_area");
		area["x"] = i->xsrc;
		area["y"] = i->ysrc;
		for(t = i->times.begin(); t != i->times.end(); ++t) {
			t->write(area.add_child("time"));
		}

		for(t = i->illuminated_times.begin(); t != i->illuminated_times.end(); ++t) {
			t->write(area.add_child("illuminated_time"));
		}
	}
}

const time_of_day& gamestatus::get_time_of_day_turn(int nturn) const
{
	if(times_.empty() == false) {
		return times_[(nturn-1)%times_.size()];
	} else {
		config dummy_cfg;
		const static time_of_day default_time(dummy_cfg);
		return default_time;
	}
}

const time_of_day& gamestatus::get_time_of_day() const
{
	return get_time_of_day_turn(turn());
}

const time_of_day& gamestatus::get_previous_time_of_day() const
{
	return get_time_of_day_turn(turn()-1);
}

const time_of_day& gamestatus::get_time_of_day(bool illuminated, const gamemap::location& loc) const
{
	for(std::vector<area_time_of_day>::const_iterator i = areas_.begin(); i != areas_.end(); ++i) {
		if(i->hexes.count(loc) == 1) {
			if(illuminated && i->illuminated_times.empty() == false) {
				return i->illuminated_times[(turn()-1)%i->illuminated_times.size()];
			} else if(i->times.empty() == false) {
				return i->times[(turn()-1)%i->times.size()];
			}
		}
	}

	if(illuminated && illuminatedTimes_.empty() == false) {
		return illuminatedTimes_[(turn()-1)%illuminatedTimes_.size()];
	} else if(times_.empty() == false) {
		return times_[(turn()-1)%times_.size()];
	}

	config dummy_cfg;
	const static time_of_day default_time(dummy_cfg);
	return default_time;
}

size_t gamestatus::turn() const
{
	return turn_;
}

int gamestatus::number_of_turns() const
{
	return numTurns_;
}

bool gamestatus::next_turn()
{
	++turn_;
	return numTurns_ == -1 || turn_ <= size_t(numTurns_);
}

game_state read_game(game_data& data, const config* cfg)
{
	log_scope("read_game");
	game_state res;
	res.label = (*cfg)["label"];
	res.version = (*cfg)["version"];
	res.gold = atoi((*cfg)["gold"].c_str());
	res.scenario = (*cfg)["scenario"];

	res.difficulty = (*cfg)["difficulty"];
	if(res.difficulty.empty())
		res.difficulty = "NORMAL";

	res.campaign_type = (*cfg)["campaign_type"];
	if(res.campaign_type.empty())
		res.campaign_type = "scenario";

	const config::child_list& units = cfg->get_children("unit");
	for(config::child_list::const_iterator i = units.begin(); i != units.end(); ++i) {
		res.available_units.push_back(unit(data,**i));
	}

	const config* const vars = cfg->child("variables");
	if(vars != NULL) {
		res.variables = vars->values;
	}

	const config* const replay = cfg->child("replay");
	if(replay != NULL) {
		res.replay_data = *replay;
	}

	const config* snapshot = cfg->child("snapshot");

	//older save files used to use 'start', so still support that for now
	if(snapshot == NULL) {
		snapshot = cfg->child("start");
	}

	if(snapshot != NULL) {
		res.snapshot = *snapshot;
	}

	const config* replay_start = cfg->child("replay_start");
	if(replay_start != NULL) {
		res.starting_pos = *replay_start;
	}

	res.can_recruit.clear();

	const std::string& can_recruit_str = (*cfg)["can_recruit"];
	if(can_recruit_str != "") {
		const std::vector<std::string> can_recruit = config::split(can_recruit_str);
		std::copy(can_recruit.begin(),can_recruit.end(),std::inserter(res.can_recruit,res.can_recruit.end()));
	}

	statistics::fresh_stats();
	if(cfg->child("statistics")) {
		statistics::read_stats(*cfg->child("statistics"));
	}

	return res;
}

void write_game(const game_state& game, config& cfg)
{
	log_scope("write_game");
	cfg["label"] = game.label;
	cfg["version"] = game_config::version;

	char buf[50];
	sprintf(buf,"%d",game.gold);
	cfg["gold"] = buf;

	cfg["scenario"] = game.scenario;

	cfg["campaign_type"] = game.campaign_type;

	cfg["difficulty"] = game.difficulty;

	cfg.add_child("variables").values = game.variables;

	for(std::vector<unit>::const_iterator i = game.available_units.begin();
	    i != game.available_units.end(); ++i) {
		config new_cfg;
		i->write(new_cfg);
		cfg.add_child("unit",new_cfg);
	}

	if(game.replay_data.child("replay") == NULL) {
		cfg.add_child("replay",game.replay_data);
	}

	cfg.add_child("snapshot",game.snapshot);
	cfg.add_child("replay_start",game.starting_pos);

	std::stringstream can_recruit;
	std::copy(game.can_recruit.begin(),game.can_recruit.end(),std::ostream_iterator<std::string>(can_recruit,","));
	std::string can_recruit_str = can_recruit.str();

	//remove the trailing comma
	if(can_recruit_str.size() > 0)
		can_recruit_str.resize(can_recruit_str.size()-1);

	cfg["can_recruit"] = can_recruit_str;

	cfg.add_child("statistics",statistics::write_stats());
}

//a structure for comparing to save_info objects based on their modified time.
//if the times are equal, will order based on the name
struct save_info_less_time {
	bool operator()(const save_info& a, const save_info& b) const {
		return a.time_modified > b.time_modified ||
		       a.time_modified == b.time_modified && a.name > b.name;
	}
};

std::vector<save_info> get_saves_list()
{
	const std::string& saves_dir = get_saves_dir();

	std::vector<std::string> saves;
	get_files_in_dir(saves_dir,&saves);

	std::vector<save_info> res;
	for(std::vector<std::string>::iterator i = saves.begin(); i != saves.end(); ++i) {
		const time_t modified = file_create_time(saves_dir + "/" + *i);

		std::replace(i->begin(),i->end(),'_',' ');
		res.push_back(save_info(*i,modified));
	}

	std::sort(res.begin(),res.end(),save_info_less_time());

	return res;
}

bool save_game_exists(const std::string& name)
{
	std::string fname = name;
	std::replace(fname.begin(),fname.end(),' ','_');
	
	return file_exists(get_saves_dir() + "/" + fname);
}

void delete_game(const std::string& name)
{
	std::string modified_name = name;
	std::replace(modified_name.begin(),modified_name.end(),' ','_');

	remove((get_saves_dir() + "/" + name).c_str());
	remove((get_saves_dir() + "/" + modified_name).c_str());
}

void load_game(game_data& data, const std::string& name, game_state& state)
{
	log_scope("load_game");
	std::string modified_name = name;
	std::replace(modified_name.begin(),modified_name.end(),' ','_');

	//try reading the file both with and without underscores
	std::string file_data = read_file(get_saves_dir() + "/" + modified_name);
	if(file_data.empty()) {
		file_data = read_file(get_saves_dir() + "/" + name);
	}

	config cfg(file_data);
	state = read_game(data,&cfg);
}

//throws gamestatus::save_game_failed
void save_game(const game_state& state)
{
	log_scope("save_game");
	std::string name = state.label;
	std::replace(name.begin(),name.end(),' ','_');

	config cfg;
	try {
		write_game(state,cfg);
		write_file(get_saves_dir() + "/" + name,cfg.write());
	} catch(io_exception& e) {
		throw gamestatus::save_game_failed(e.what());
	};
}
