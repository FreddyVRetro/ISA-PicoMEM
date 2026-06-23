
#pragma once

	struct {
		uint8_t index;
		uint8_t dac[2],fm[2],cda[2],master[2],lin[2];
		uint8_t mic;
		bool stereo;
		bool enabled;
		bool filter_bypass;
		bool sbpro_stereo; /* Game or OS is attempting SB Pro type stereo */
		uint8_t unhandled[0x48];
		uint8_t ess_id_str[4];
		uint8_t ess_id_str_pos;
	} mixer_t;