#include "includes.h"
#include <optional>

Resolver g_resolver{};;

LagRecord* Resolver::FindIdealRecord( AimPlayer* data ) {
	LagRecord* first_valid, * current;

	if( data->m_records.empty( ) || !data )
		return nullptr;

	first_valid = nullptr;

	// iterate records.
	for( const auto& it : data->m_records ) {
		if( it->dormant( ) || it->immune( ) || !it->valid( ) )
			continue;

		// get current record.
		current = it.get( );

		// first record that was valid, store it for later.
		if( !first_valid )
			first_valid = current;

		// try to find a record with a shot, lby update, walking or no anti-aim.
		if( it->m_shot || it->m_mode == Modes::RESOLVE_BODY || it->m_mode == Modes::RESOLVE_WALK || it->m_mode == Modes::RESOLVE_NONE )
			return current;
	}

	// none found above, return the first valid record if possible.
	return ( first_valid ) ? first_valid : nullptr;
}

LagRecord* Resolver::FindLastRecord( AimPlayer* data ) {
	LagRecord* current;

	if( data->m_records.empty( ) || !data )
		return nullptr;

	// iterate records in reverse.
	for( auto it = data->m_records.crbegin( ); it != data->m_records.crend( ) - 1; ++it ) {
		if( it->get( )->dormant( ) )
			continue;

		current = it->get( );

		// if this record is valid.
		// we are done since we iterated in reverse.
		if( current->valid( ) && !current->immune( ) )
			return current;
	}

	return nullptr;
}

// middle
LagRecord* Resolver::FindFirstRecord( AimPlayer* data ) {
	LagRecord* current;

	if( data->m_records.empty( ) || !data )
		return nullptr;

	// iterate records in reverse.
	for( auto it = data->m_records.crbegin( ); it != data->m_records.crend( ) - ( data->m_records.size( ) / 2.f ); ++it ) {
		if( it->get( )->dormant( ) )
			continue;

		current = it->get( );

		// if this record is valid.
		// we are done since we iterated in reverse.
		if( current->valid( ) && !current->immune( ) )
			return current;
	}

	return nullptr;
}

void Resolver::OnBodyUpdate( Player* player, float value ) {
	AimPlayer* data = &g_aimbot.m_players[ player->index( ) - 1 ];

	// set data.
	data->m_old_body = data->m_body;
	data->m_body = value;
}

float Resolver::GetAwayAngle( LagRecord* record ) {
	float  delta{ std::numeric_limits< float >::max( ) };
	vec3_t pos;
	ang_t  away;

	// other cheats predict you by their own latency.
	// they do this because, then they can put their away angle to exactly
	// where you are on the server at that moment in time.

	// the idea is that you would need to know where they 'saw' you when they created their user-command.
	// lets say you move on your client right now, this would take half of our latency to arrive at the server.
	// the delay between the server and the target client is compensated by themselves already, that is fortunate for us.

	// we have no historical origins.
	// no choice but to use the most recent one.
	//if( g_cl.m_net_pos.empty( ) ) {
	math::VectorAngles( g_cl.m_local->m_vecOrigin( ) - record->m_origin, away );
	return away.y;
	//}

	// half of our rtt.
	// also known as the one-way delay.
	//float owd = ( g_cl.m_latency / 2.f );

	// since our origins are computed here on the client
	// we have to compensate for the delay between our client and the server
	// therefore the OWD should be subtracted from the target time.
	//float target = record->m_pred_time; //- owd;

	// iterate all.
	//for( const auto &net : g_cl.m_net_pos ) {
		// get the delta between this records time context
		// and the target time.
	//	float dt = std::abs( target - net.m_time );

		// the best origin.
	//	if( dt < delta ) {
	//		delta = dt;
	//		pos   = net.m_pos;
	//	}
	//}

	//math::VectorAngles( pos - record->m_pred_origin, away );
	//return away.y;
}

void Resolver::MatchShot( AimPlayer* data, LagRecord* record ) {
	// do not attempt to do this in nospread mode.
	if( !g_hooks.b[ XOR( "antiUntrusted" ) ] )
		return;

	float shoot_time = -1.f;

	Weapon* weapon = data->m_player->GetActiveWeapon( );
	if( weapon ) {
		// with logging this time was always one tick behind.
		// so add one tick to the last shoot time.
		shoot_time = weapon->m_fLastShotTime( ) + g_csgo.m_globals->m_interval;
	}

	// this record has a shot on it.
	if( game::TIME_TO_TICKS( shoot_time ) == game::TIME_TO_TICKS( record->m_sim_time ) ) {
		if( record->m_lag <= 2 )
			record->m_shot = true;

		// more then 1 choke, cant hit pitch, apply prev pitch.
		else if( data->m_records.size( ) >= 2 ) {
			LagRecord* previous = data->m_records[ 1 ].get( );

			if( previous && !previous->dormant( ) )
				record->m_eye_angles.x = previous->m_eye_angles.x;
		}
	}
}

void Resolver::AntiFreestand( LagRecord* record ) {
	// constants
	constexpr float STEP{ 4.f };
	constexpr float RANGE{ 32.f };

	// best target.
	vec3_t enemypos = record->m_player->GetShootPorsition( );
	float away = GetAwayAngle( record );

	// construct vector of angles to test.
	std::vector< AdaptiveAngle > angles{ };
	angles.emplace_back( away - 180.f );
	angles.emplace_back( away + 90.f );
	angles.emplace_back( away - 90.f );

	// start the trace at the your shoot pos.
	vec3_t start = g_cl.m_local->GetShootPorsition( );

	// see if we got any valid result.
	// if this is false the path was not obstructed with anything.
	bool valid{ false };

	// iterate vector of angles.
	for( auto it = angles.begin( ); it != angles.end( ); ++it ) {

		// compute the 'rough' estimation of where our head will be.
		vec3_t end{ enemypos.x + std::cos( math::deg_to_rad( it->m_yaw ) ) * RANGE,
			enemypos.y + std::sin( math::deg_to_rad( it->m_yaw ) ) * RANGE,
			enemypos.z };

		// draw a line for debugging purposes.
		// g_csgo.m_debug_overlay->AddLineOverlay(  start, end, 255, 0, 0, true, 0.1f  );

		// compute the direction.
		vec3_t dir = end - start;
		float len = dir.normalize( );

		// should never happen.
		if( len <= 0.f )
			continue;

		// step thru the total distance, 4 units per step.
		for( float i{ 0.f }; i < len; i += STEP ) {
			// get the current step position.
			vec3_t point = start + ( dir * i );

			// get the contents at this point.
			int contents = g_csgo.m_engine_trace->GetPointContents( point, MASK_SHOT_HULL );

			// contains nothing that can stop a bullet.
			if( !( contents & MASK_SHOT_HULL ) )
				continue;

			float mult = 1.f;

			// over 50% of the total length, prioritize this shit.
			if( i > ( len * 0.5f ) )
				mult = 1.25f;

			// over 90% of the total length, prioritize this shit.
			if( i > ( len * 0.75f ) )
				mult = 1.25f;

			// over 90% of the total length, prioritize this shit.
			if( i > ( len * 0.9f ) )
				mult = 2.f;

			// append 'penetrated distance'.
			it->m_dist += ( STEP * mult );

			// mark that we found anything.
			valid = true;
		}
	}

	if( !valid ) {
		// set angle to backwards.
		record->m_eye_angles.y = math::NormalizedAngle( away + 180.f );
		return;
	}

	// put the most distance at the front of the container.
	std::sort( angles.begin( ), angles.end( ),
			   [ ]( const AdaptiveAngle& a, const AdaptiveAngle& b ) {
		return a.m_dist > b.m_dist;
	} );

	// the best angle should be at the front now.
	AdaptiveAngle* best = &angles.front( );

	record->m_eye_angles.y = math::NormalizedAngle( best->m_yaw );
}

bool IsTickValid( int tick ) {
	// better use polak's version than our old one, getting more accurate results
	auto nci = g_csgo.m_engine->GetNetChannelInfo( );

	static auto sv_maxunlag = g_csgo.m_cvar->FindVar( HASH( "sv_maxunlag" ) );
	if( !nci || !sv_maxunlag )
		return false;

	float correct = std::clamp( nci->GetLatency( INetChannel::FLOW_OUTGOING ) + g_cl.m_lerp, 0.f, sv_maxunlag->GetFloat( ) );

	float deltaTime = correct - ( g_csgo.m_globals->m_curtime - game::TICKS_TO_TIME( tick ) );

	return fabsf( deltaTime ) < 0.2f;
}

void AimPlayer::SetOverwriteTick( Player* player, ang_t angles, float_t correct_time, LagRecord* record ) {
	if( !IsTickValid( game::TIME_TO_TICKS( correct_time ) ) ) {
		g_notify.add( tfm::format( XOR( "Dev Error: failed to overwrite tick, delta too big.\n" ) ) );
	}

	record->store( player );
	record->m_eye_angles = angles;
	record->m_sim_time = correct_time;
	m_records.emplace_back( record );
}

void Resolver::ResolveOverride( AimPlayer* data, LagRecord* record, Player* player ) {
	if( g_hooks.is_key_down( g_hooks.i[ XOR( "override_key" ) ] ) && g_hooks.b[ XOR( "override" ) ] ) {
		ang_t       viewangles;
		g_csgo.m_engine->GetViewAngles( viewangles );

		//auto yaw = math::clamp (  g_cl.m_local->GetAbsOrigin(   ), Player->origin(   )  ).y;
		const float at_target_yaw = math::CalcAngle( g_cl.m_local->m_vecOrigin( ), record->m_origin ).y;

		if( fabs( math::NormalizedAngle( viewangles.y - at_target_yaw ) ) > 30.f )
			return ResolveStand( data, record );

		//record->m_eye_angles.y = (  math::NormalizedAngle(  viewangles.y - at_target_yaw  ) > 0  ) ? at_target_yaw + 90.f : at_target_yaw - 90.f;

		if( ( math::NormalizedAngle( viewangles.y - at_target_yaw ) > 0 && math::NormalizedAngle( viewangles.y - at_target_yaw ) <= 2.f ) || ( math::NormalizedAngle( viewangles.y - at_target_yaw ) < 0 && math::NormalizedAngle( viewangles.y - at_target_yaw ) > -2.f ) ) {
			record->m_eye_angles.y = at_target_yaw;
		}
		else if( math::NormalizedAngle( viewangles.y - at_target_yaw ) < 0 ) {
			record->m_eye_angles.y = at_target_yaw - 90.f;
		}
		else if( math::NormalizedAngle( viewangles.y - at_target_yaw ) > 0 ) {
			record->m_eye_angles.y = at_target_yaw + 90.f;
		}

		// set the resolve mode.
		resolver_state[ player->index( ) ] = XOR( "RESOLVE_OVERRIDE" );
		record->m_mode = Modes::RESOLVE_OVERRIDE;
	}

	if( record->m_anim_time > data->m_body_update && g_hooks.b[ XOR( "resolver" ) ] ) {
		// only shoot the LBY flick 3 times.
		// if we happen to miss then we most likely mispredicted.
		if( data->m_body_index <= 0 ) {
			// set angles to current LBY.
			record->m_eye_angles.y = data->m_body;

			// predict next body update.
			data->m_body_update = record->m_anim_time + 1.1f;

			// set the resolve mode.
			resolver_state[ player->index( ) ] = XOR( "RESOLVER_BODY" );
			record->m_mode = Modes::RESOLVE_BODY;

			return;
		}
		else if( data->m_body != data->m_old_body && !( data->m_body_index <= 0 ) && g_hooks.b[ XOR( "resolver" ) ] ) {
			// set angles to current LBY.
			record->m_eye_angles.y = data->m_body;

			// predict next body update.
			data->m_body_update = record->m_anim_time + 1.1f;

			// reset misses.
			data->m_body_index = 0;

			// set the resolve mode.
			resolver_state[ player->index( ) ] = XOR( "RESOLVER_BODY" );
			record->m_mode = Modes::RESOLVE_BODY;

			return;
		}
	}
}

void Resolver::SetMode( LagRecord* record ) {
	// the resolver has 3 modes to chose from.
	// these modes will vary more under the hood depending on what data we have about the player
	// and what kind of hack vs. hack we are playing (mm/nospread).

	float speed = record->m_velocity.length_2d( );

	// if on ground, moving, and not fakewalking.
	if( ( record->m_flags & FL_ONGROUND ) && speed > 0.1f && !record->m_fake_walk )
		record->m_mode = Modes::RESOLVE_WALK;

	// if on ground, not moving or fakewalking.
	else if( ( record->m_flags & FL_ONGROUND ) && ( speed <= 0.1f || record->m_fake_walk ) && !g_hooks.is_key_down( g_hooks.i[ XOR( "override_key" ) ] ) )
		record->m_mode = Modes::RESOLVE_STAND;

	// if on ground, not moving or fakewalking.
	if( ( record->m_flags & FL_ONGROUND ) && ( speed <= 0.1f || record->m_fake_walk ) && g_hooks.is_key_down( g_hooks.i[ XOR( "override_key" ) ] ) && g_hooks.b[ XOR( "override" ) ] )
		record->m_mode = Modes::RESOLVE_OVERRIDE;

	// if not on ground.
	else if( !( record->m_flags & FL_ONGROUND ) )
		record->m_mode = Modes::RESOLVE_AIR;
}

void Resolver::ResolveAngles( Player* player, LagRecord* record ) {
	AimPlayer* data = &g_aimbot.m_players[ player->index( ) - 1 ];

	// mark this record if it contains a shot.
	MatchShot( data, record );

	// next up mark this record with a resolver mode that will be used.
	SetMode( record );

	// if we are in nospread mode, force all players pitches to down.
	// TODO; we should check thei actual pitch and up too, since those are the other 2 possible angles.
	// this should be somehow combined into some iteration that matches with the air angle iteration.
	if( !g_hooks.b[ XOR( "antiUntrusted" ) ] ) {
		constexpr float angles[ 3 ] = { -89, 0, 89 };
		record->m_eye_angles.x = angles[ std::clamp( data->m_nospread_shots % 5, 0, 2 ) ];
	}

	// we arrived here we can do the acutal resolve.
	if( record->m_mode == Modes::RESOLVE_WALK )
		ResolveWalk( data, record );

	else if( record->m_mode == Modes::RESOLVE_STAND && !g_hooks.is_key_down( g_hooks.i[ XOR( "override_key" ) ] ) )
		ResolveStand( data, record );

	else if( record->m_mode == Modes::RESOLVE_OVERRIDE && g_hooks.is_key_down( g_hooks.i[ XOR( "override_key" ) ] ) && g_hooks.b[ XOR( "override" ) ] )
		ResolveOverride( data, record, player );

	else if( record->m_mode == Modes::RESOLVE_AIR )
		ResolveAir( data, record );

	// normalize the eye angles, doesn't really matter but its clean.
	math::NormalizeAngle( record->m_eye_angles.y );
}

void Resolver::ResolveWalk( AimPlayer* data, LagRecord* record ) {
	// apply lby to eyeangles.
	record->m_eye_angles.y = record->m_body;

	// delay body update.
	data->m_body_update = record->m_anim_time + 0.22f;

	// reset stand and body index.
	data->m_stand_index = 0;
	data->m_stand_index2 = 0;
	data->m_body_index = 0;

	resolver_state[ record->m_player->index( ) ] = XOR( "RESOLVE_WALK" );

	// copy the last record that this player was walking
	// we need it later on because it gives us crucial data.
	std::memcpy( &data->m_walk_record, record, sizeof( LagRecord ) );
}

bool Resolver::IsYawSideways( LagRecord* record, float yaw ) {
	const auto at_target_yaw = math::CalcAngle( g_cl.m_local->m_vecOrigin( ), record->m_origin ).y;
	const float delta = fabs( math::NormalizedAngle( at_target_yaw - yaw ) );

	return delta > 20.f && delta < 160.f;
}

bool Resolver::IsYawBackwards( LagRecord* record, float yaw ) {
	auto at_target_yaw = math::calc_angle( g_cl.m_local->m_vecOrigin( ), record->m_origin ).y;
	math::NormalizeAngle( at_target_yaw );
	float delta = fabs( math::NormalizedAngle( at_target_yaw - yaw ) );

	return  delta >= 0.f && delta < 17.f;
}

bool IsAdjustingBalance( Player* player, LagRecord* record, C_AnimationLayer* layer ) {
	for( int i = 0; i < 13; i++ ) {
		const int activity = player->GetSequenceActivity( record->m_layers[ i ].m_sequence );
		if( activity == ACT_CSGO_IDLE_TURN_BALANCEADJUST ) {
			*layer = record->m_layers[ i ];
			return true;
		}
	}
	return false;
}

float GetLBYRotatedYaw( float lby, float yaw ) {
	float delta = math::NormalizedAngle( yaw - lby );
	if( fabs( delta ) < 25.f )
		return lby;

	if( delta > 0.f )
		return yaw + 25.f;

	return yaw;
}

void ResolveYawBruteforce( AimPlayer* data, LagRecord* record ) {
	auto local_player = g_cl.m_local;
	if( !local_player )
		return;

	auto& resolve_record = data;
	g_resolver.resolver_state[ record->m_player->index( ) ] = XOR( "RESOLVE_BRUTE" );

	const float at_target_yaw = math::CalcAngle( record->m_origin, local_player->m_vecOrigin( ) ).y;

	switch( data->m_stand_index % 3 ) {
		case 0:
			record->m_eye_angles.y = GetLBYRotatedYaw( record->m_body, at_target_yaw + 60.f );
			break;
		case 1:
			record->m_eye_angles.y = at_target_yaw + 140.f;
			break;
		case 2:
			record->m_eye_angles.y = at_target_yaw - 75.f;
			break;
	}
}

void Resolver::ResolveStand( AimPlayer* data, LagRecord* record ) {
	// for no-spread call a seperate resolver.
	if( !g_hooks.b[ XOR( "antiUntrusted" ) ] ) {
		StandNS( data, record );
		return;
	}

	// get predicted away angle for the player.
	float away = GetAwayAngle( record );
	auto flForward = away - 180.f;
	const float flYaw = abs( record->m_eye_angles.y - flForward );

	auto flDelta = math::NormalizedAngle( ( away + 180.f ) - data->m_body );
	auto bBackwards = fabs( flDelta ) < 70.f;

	const float flBodyDelta = fabs( math::NormalizedAngle( away - data->m_body ) );

	// pointer for easy access.
	LagRecord* move = &data->m_walk_record;

	// we have a valid moving record.
	if( move->m_sim_time > 0.f && !move->dormant( ) && !record->dormant( ) ) {
		vec3_t delta = move->m_origin - record->m_origin;

		// check if moving record is close.
		if( delta.length_2d( ) <= 128.f ) {
			// indicate that we are using the moving lby.
			data->m_moved = true;
		}
	}

	// a valid moving context was found
	if( data->m_moved ) {
		if( g_hooks.b[ XOR( "resolver" ) ] ) {
			// LBY SHOULD HAVE UPDATED HERE.
			if( !g_cl.m_lag && /*data->m_body != data->m_old_body || */ record->m_anim_time >= data->m_body_update ) {
				// only shoot the LBY flick 3 times.
				// if we happen to miss then we most likely mispredicted
				if( data->m_body_index < 2 ) {
					// set angles to current LBY.
					record->m_eye_angles.y = record->m_body;

					// predict next body update.
					data->m_body_update = record->m_anim_time + 1.1f;

					// set the resolve mode.
					resolver_state[ record->m_player->index( ) ] = XOR( "RESOLVE_BODY" );
					record->m_mode = Modes::RESOLVE_BODY;

					return;
				}
			}
		}

		// set to stand1 -> known last move.
		record->m_mode = Modes::RESOLVE_STAND;

		C_AnimationLayer* curr = &record->m_layers[ 3 ];
		int act = data->m_player->GetSequenceActivity( curr->m_sequence );

		// ok, no fucking update. apply big resolver.
		//IsYawSideways( record, move->m_body ) ? resolver_state[ record->m_player->index( ) ] = XOR( "RESOLVE_FREESTAND" ) : resolver_state[ record->m_player->index( ) ] = XOR( "RESOLVE_LASTMOVE" );
		//IsYawSideways( record, move->m_body ) ? AntiFreestand( record ) : record->m_eye_angles.y = move->m_body;

		// edge yaw // ??
		ang_t ang;
		if( data->m_stand_index == 0 && g_hooks.b[ XOR( "aa_edge_correction" ) ] && g_hvh.DoEdgeAntiAim( data->m_player, ang ) ) {
			record->m_eye_angles.y = ang.y;
			return;
		}

		AntiFreestand( record );
		resolver_state[ record->m_player->index( ) ] = XOR( "RESOLVE_FREESTAND" );

		float change = std::abs( math::NormalizedAngle( record->m_body - record->m_eye_angles.y ) );
		/*if( IsYawSideways( record, record->m_body ) && change < 35.f ) {
			record->m_eye_angles.y = record->m_body;

			resolver_state[ record->m_player->index( ) ] = XOR( "RESOLVE_SIDEWAYS" );
		}
		else */if( IsYawBackwards( record, record->m_body ) && data->m_stand_index == 0 ) {
			record->m_eye_angles.y = away + 180.f;

			resolver_state[ record->m_player->index( ) ] = XOR( "RESOLVE_BACKWARDS" );
		}

		// jesus we can fucking stop missing can we?
		if( data->m_stand_index > 2 ) {
			switch( data->m_stand_index % 6 ) {
				case 0:
					AntiFreestand( record );

					resolver_state[ record->m_player->index( ) ] = XOR( "RESOLVE_FREESTAND" );
					break;

				case 1:
					record->m_eye_angles.y = away + 70.f;

					resolver_state[ record->m_player->index( ) ] = XOR( "RESOLVE_BACKWARDS" );
					break;

				case 2:
					record->m_eye_angles.y = away - 70.f;

					resolver_state[ record->m_player->index( ) ] = XOR( "RESOLVE_SIDEWAYS(+)" );
					break;

				case 3:
					record->m_eye_angles.y = away + 180.f;

					resolver_state[ record->m_player->index( ) ] = XOR( "RESOLVE_SIDEWAYS(-)" );
					break;

				case 4:
					record->m_eye_angles.y = record->m_body;

					resolver_state[ record->m_player->index( ) ] = XOR( "RESOLVE_BODY" );
					break;

				case 5:
					record->m_eye_angles.y = move->m_body;
					
					resolver_state[ record->m_player->index( ) ] = XOR( "RESOLVE_LASTMOVE" );
					break;

				default:
					break;
			}

			//ResolveYawBruteforce( data, record );
		}

		return;
	}

	// stand2 -> no known last move.
	record->m_mode = Modes::RESOLVE_STAND2;

	if( !g_cl.m_lag && g_hooks.b[ XOR( "resolver" ) ] && data->m_body != data->m_old_body ) {
		// set angles to current LBY.
		record->m_eye_angles.y = record->m_body;

		// set the resolve mode.
		resolver_state[ record->m_player->index( ) ] = XOR( "RESOLVE_BODY" );
		record->m_mode = Modes::RESOLVE_BODY;

		return;
	}

	switch( data->m_stand_index2 % 6 ) {
		case 0:
			AntiFreestand( record );
			break;

		case 1:
			record->m_eye_angles.y = data->m_body;
			break;

		case 2:
			record->m_eye_angles.y = data->m_body + 180.f;
			break;

		case 3:
			record->m_eye_angles.y = data->m_body + 110.f;
			break;

		case 4:
			record->m_eye_angles.y = data->m_body - 110.f;
			break;

		case 5:
			record->m_eye_angles.y = away;
			break;

		default:
			break;
	}

	resolver_state[ record->m_player->index( ) ] = XOR( "RESOLVE_BRUTE" );
}

void Resolver::StandNS( AimPlayer* data, LagRecord* record ) {
	float  delta{ std::numeric_limits< float >::max( ) };
	vec3_t pos;
	ang_t  away;

	// other cheats predict you by their own latency.
	// they do this because, then they can put their away angle to exactly
	// where you are on the server at that moment in time.

	// the idea is that you would need to know where they 'saw' you when they created their user-command.
	// lets say you move on your client right now, this would take half of our latency to arrive at the server.
	// the delay between the server and the target client is compensated by themselves already, that is fortunate for us.

	// we have no historical origins.
	// no choice but to use the most recent one.
	if( g_cl.m_net_pos.empty( ) ) {
		math::VectorAngles( g_cl.m_local->m_vecOrigin( ) - record->m_origin, away );

		record->m_eye_angles.y = away.y + data->m_nospread_shots * 45.f;
		return;
	}

	// half of our rtt.
	// also known as the one-way delay.
	float owd = ( g_cl.m_latency / 2.f );

	// since our origins are computed here on the client
	// we have to compensate for the delay between our client and the server
	// therefore the OWD should be subtracted from the target time.
	float target = record->m_sim_time; //- owd;

	// iterate all.
	for( const auto& net : g_cl.m_net_pos ) {
		// get the delta between this records time context
		// and the target time.
		float dt = std::abs( target - net.m_time );

		// the best origin.
		if( dt < delta ) {
			delta = dt;
			pos = net.m_pos;
		}
	}

	math::VectorAngles( pos - record->m_origin, away );

	record->m_eye_angles.y = away.y + data->m_nospread_shots * 45.f;
	return;
}

void Resolver::ResolveAir( AimPlayer* data, LagRecord* record ) {
	// for no-spread call a seperate resolver.
	if( !g_hooks.b[ XOR( "antiUntrusted" ) ] ) {
		AirNS( data, record );
		return;
	}

	// else run our matchmaking air resolver.

	if( data && data->m_records.size( ) ) {
		LagRecord* current = data->m_records[ 0 ].get( );
		if( data->m_player->GetSequenceActivity( record->m_layers[ 4 ].m_sequence ) == 985 && data->m_records.size( ) >= 2 ) {
			LagRecord* previous = data->m_records[ 1 ].get( );
			if( data->m_player->GetSequenceActivity( previous->m_layers[ 5 ].m_sequence ) == 986 ) {
				// set this for completion.
				// so the shot parsing wont pick the hits / misses up.
				// and process them wrongly.
				record->m_mode = Modes::RESOLVE_STAND;

				// invoke our stand resolver.
				ResolveStand( data, record );

				// we are done.
				return;
			}
		}
	}

	// we have barely any speed. 
	// either we jumped in place or we just left the ground.
	// or someone is trying to fool our resolver.
	if( record->m_velocity.length_2d( ) < 60.f ) {
		// set this for completion.
		// so the shot parsing wont pick the hits / misses up.
		// and process them wrongly.
		record->m_mode = Modes::RESOLVE_STAND;

		// invoke our stand resolver.
		ResolveStand( data, record );

		// we are done.
		return;
	}

	// try to predict the direction of the player based on his velocity direction.
	// this should be a rough estimation of where he is looking.
	float velyaw = math::rad_to_deg( std::atan2( record->m_velocity.y, record->m_velocity.x ) );

	switch( data->m_shots % 4 ) {
		case 0:
			record->m_eye_angles.y = record->m_body;
			break;

		case 1:
			record->m_eye_angles.y = velyaw + 180.f;
			break;

		case 2:
			record->m_eye_angles.y = velyaw - 90.f;
			break;

		case 3:
			record->m_eye_angles.y = velyaw + 90.f;
			break;
	}

	resolver_state[ record->m_player->index( ) ] = XOR( "RESOLVE_AIR" );
}

void Resolver::AirNS( AimPlayer* data, LagRecord* record ) {
	float  delta{ std::numeric_limits< float >::max( ) };
	vec3_t pos;
	ang_t  away;

	// other cheats predict you by their own latency.
	// they do this because, then they can put their away angle to exactly
	// where you are on the server at that moment in time.

	// the idea is that you would need to know where they 'saw' you when they created their user-command.
	// lets say you move on your client right now, this would take half of our latency to arrive at the server.
	// the delay between the server and the target client is compensated by themselves already, that is fortunate for us.

	// we have no historical origins.
	// no choice but to use the most recent one.
	if( g_cl.m_net_pos.empty( ) ) {
		math::VectorAngles( g_cl.m_local->m_vecOrigin( ) - record->m_origin, away );

		record->m_eye_angles.y = away.y + data->m_nospread_shots * 45.f;
		return;
	}

	// half of our rtt.
	// also known as the one-way delay.
	float owd = ( g_cl.m_latency / 2.f );

	// since our origins are computed here on the client
	// we have to compensate for the delay between our client and the server
	// therefore the OWD should be subtracted from the target time.
	float target = record->m_sim_time; //- owd;

	// iterate all.
	for( const auto& net : g_cl.m_net_pos ) {
		// get the delta between this records time context
		// and the target time.
		float dt = std::abs( target - net.m_time );

		// the best origin.
		if( dt < delta ) {
			delta = dt;
			pos = net.m_pos;
		}
	}

	math::VectorAngles( pos - record->m_origin, away );

	record->m_eye_angles.y = away.y + data->m_nospread_shots * 45.f;
	return;
}

void Resolver::ResolvePoses( Player* player, LagRecord* record ) {
	AimPlayer* data = &g_aimbot.m_players[ player->index( ) - 1 ];

	// only do this bs when in air.
	if( record->m_mode == Modes::RESOLVE_AIR ) {
		// ang = pose min + pose val x ( pose range )

		// lean_yaw
		player->m_flPoseParameter( )[ 2 ] = g_csgo.RandomInt( 0, 4 ) * 0.25f;

		// body_yaw
		player->m_flPoseParameter( )[ 11 ] = g_csgo.RandomInt( 1, 3 ) * 0.25f;
	}
}