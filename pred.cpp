#include "includes.h"
#include "pred.h"

InputPrediction g_inputpred{};;

void InputPrediction::update() {
	bool        valid{ g_csgo.m_cl->m_delta_tick > 0 };
	//int         outgoing_command, current_command;
	//CUserCmd    *cmd;

	// render start was not called.
	if (g_cl.m_stage == FRAME_NET_UPDATE_END) {
		/*outgoing_command = g_csgo.m_cl->m_last_outgoing_command + g_csgo.m_cl->m_choked_commands;

		// this must be done before update ( update will mark the unpredicted commands as predicted ).
		for( int i{}; ; ++i ) {
			current_command = g_csgo.m_cl->m_last_command_ack + i;

			// caught up / invalid.
			if( current_command > outgoing_command || i >= MULTIPLAYER_BACKUP )
				break;

			// get command.
			cmd = g_csgo.m_input->GetUserCmd( current_command );
			if( !cmd )
				break;

			// cmd hasn't been predicted.
			// m_nTickBase is incremented inside RunCommand ( which is called frame by frame, we are running tick by tick here ) and prediction hasn't run yet,
			// so we must fix tickbase by incrementing it ourselves on non-predicted commands.
			if( !cmd->m_predicted )
				++g_cl.m_local->m_nTickBase( );
		}*/

		// EDIT; from what ive seen RunCommand is called when u call Prediction::Update
		// so the above code is not fucking needed.

		int start = g_csgo.m_cl->m_last_command_ack;
		int stop = g_csgo.m_cl->m_last_outgoing_command + g_csgo.m_cl->m_choked_commands;

		// call CPrediction::Update.
		g_csgo.m_prediction->Update(g_csgo.m_cl->m_delta_tick, valid, start, stop);
	}

	if (g_cl.m_cmd)
		g_netdata.apply(g_cl.m_cmd->m_command_number - 1);
}

void InputPrediction::run() {

	auto angles = &g_cl.m_cmd->m_view_angles;
	const auto pre_angles = *angles;

	auto in_pred = &g_csgo.m_prediction->m_in_prediction;
	const auto in_prediction_old = *in_pred;
	*in_pred = 1;

	auto  predicted_commands = &g_csgo.m_prediction->m_is_first_time_predicted;
	const auto predicted_commands_old = *predicted_commands;
	*predicted_commands = 0;

	auto v9 = (float*)(uintptr_t(g_csgo.sv_footsteps) + 0x2C);
	const auto sv_footsteps_old = *v9;

	auto v10 = (float*)(uintptr_t(g_csgo.sv_min_jump_landing_sound) + 0x2C);
	const auto sv_min_jump_landing_sound_old = *v10;

	auto v11 = (int*)(uintptr_t(g_csgo.m_game_movement) + 8);
	const auto move_data_old = *v11;

	auto m_vecPreviousPredictedOrigin = (vec3_t*)(uintptr_t(g_cl.m_local) + 0x35A8);
	const auto m_vecPreviouslyPredictedOrigin_old = *m_vecPreviousPredictedOrigin;

	for (auto i = g_csgo.sv_footsteps; i != i->m_parent; i = i->m_parent)
		*(float*)(uintptr_t(i) + 0x2C) = (uint32_t)i ^ uint32_t(0.0f);

	for (auto j = g_csgo.sv_min_jump_landing_sound; j != j->m_parent; j = j->m_parent)
		*(float*)(uintptr_t(j) + 0x2C) = (uint32_t)j ^ 0x7F7FFFFF;

	m_perfect_accuracy = 0.f;

	// CPrediction::StartCommand
	g_cl.m_local->m_pCurrentCommand() = g_cl.m_cmd;
	*g_csgo.m_nPredictionRandomSeed = g_cl.m_cmd->m_random_seed;
	g_csgo.m_pPredictionPlayer = g_cl.m_local;

	// backup globals.
	m_curtime = g_csgo.m_globals->m_curtime;
	m_frametime = g_csgo.m_globals->m_frametime;

	// restore original angles after input prediction
	*angles = g_cl.m_view_angles;

	// CPrediction::RunCommand

	// set globals appropriately.
	g_csgo.m_globals->m_curtime = game::TICKS_TO_TIME(g_cl.m_local->m_nTickBase());
	g_csgo.m_globals->m_frametime = g_cl.m_local->m_fFlags() & 0x40 ? 0.f : g_csgo.m_globals->m_interval;

	// set target player ( host ).
	g_csgo.m_move_helper->SetHost(g_cl.m_local);
	g_csgo.m_game_movement->StartTrackPredictionErrors(g_cl.m_local);

	// setup input.
	g_csgo.m_prediction->SetupMove(g_cl.m_local, g_cl.m_cmd, g_csgo.m_move_helper, &data);

	// run movement.
	g_csgo.m_game_movement->ProcessMovement(g_cl.m_local, &data);
	g_csgo.m_prediction->FinishMove(g_cl.m_local, g_cl.m_cmd, &data);
	g_csgo.m_game_movement->FinishTrackPredictionErrors(g_cl.m_local);

	*in_pred = in_prediction_old;
	*predicted_commands = predicted_commands_old;
	*g_csgo.m_nPredictionRandomSeed = -1;
	g_csgo.m_pPredictionPlayer = nullptr;
	g_cl.m_local->m_pCurrentCommand() = 0;
	*v9 = sv_footsteps_old;
	*v10 = sv_min_jump_landing_sound_old;
	*v11 = move_data_old;
	*m_vecPreviousPredictedOrigin = m_vecPreviouslyPredictedOrigin_old;

	*angles = pre_angles;

	// reset target player ( host ).
	g_csgo.m_move_helper->SetHost(nullptr);
}

void InputPrediction::restore() {
	// restore globals.
	g_csgo.m_globals->m_curtime = m_curtime;
	g_csgo.m_globals->m_frametime = m_frametime;
}