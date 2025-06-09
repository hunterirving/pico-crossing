#ifndef TOWN_TUNES_HPP
#define TOWN_TUNES_HPP

#include <vector>
#include <string>
#include <cstring>
#include "gcReport.hpp"
#include "types.hpp"

// Function to check if a UTF-8 character is the frog emoji
bool isFrogCharacter(const Utf8Char& c);

namespace TownTunes {
	// Town tune data structure - each string represents a 16-note melody
	const std::vector<std::string> tunes = {
		"Gfefecg_agabcG__", //pkmn route 201
		"cGfe-_dedefG--Ae", //pkmn littleroot town 
		"gdfGfdcdgdfGfdcd", //billie jean
		"GGfG_d_dGCBG____", //funky town
		"AAfd_d_G_G_GBBCD", //A-HA Take On Me *ABSOLUTE BANGER
		"G-DBB-AGGC-BBAAG", //smash mouth all star 
		"D_B_AG_B-CB-AGed", //smash mouth allstar version 2
		"dGAB-BDA-ABG-___", //smiles and tears
		"f_f_fed-f_f_fed-", //this is halloween
		"cdeGCGedabceAecb", //final fantasy prelude
		"GdbGdbGdfdafdafd", //coldplay - clocks
		"acACECACGGCGDCBC", //daft punk - harder better faster stronger
		"ee_e_ce_G___g___", //super mario bros theme
		"dcbcddd_eee-ddd_", //pokemon victory theme
		"f_C_E_B_f_C_E_B_", //lavender town
		"CBC-G-f-CBC-G-f-", //jurasic park
		"cgcG-f-edb-_____", //pkmn center theme
		"E-BCD-CBA-ACE-__", //tetris
		"gGfefe-cgacfCBCG", //animal crossing new leaf
		"def_d_B--_A--___", //tom nook's theme
		"C--f--B---CDCAfB", //back to the future
		"f__A_B_DC__A_f__", //the simpsons
		"feda---_edcg---_", //pikmin forest of hope
	};

	// Note values in game order
	const char NOTE_ORDER[] = {'_', '-', 'g', 'a', 'b', 'c', 'd', 'e', 'f', 'G', 'A', 'B', 'C', 'D', 'E', '?'};

	// Find the index of a note in the NOTE_ORDER array
	inline int getNoteIndex(char note) {
		for (int i = 0; i < 16; i++) {
			if (NOTE_ORDER[i] == note) {
				return i;
			}
		}
		return 0; // Default to silent if note not found
	}

	// Public interface functions
	// Check if currently in town tune mode
	bool isInTownTuneMode();

	// Get the current tune index
	int getCurrentTuneIndex();

	// Check if tune has been completed
	bool isTuneCompleted();

	// Enter town tune mode
	void enterTownTuneMode();

	// Process town tune state machine
	void processTownTune(GCReport& report, bool xJustPressed, bool leftJustPressed, 
					bool rightJustPressed, bool startPressed, int64_t hold_duration_us);

	// Exit town tune mode
	void exitTownTuneMode();
}

#endif
