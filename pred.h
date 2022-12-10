#pragma once

class InputPrediction {
public:
	CMoveData data{};

	float m_curtime;
	float m_frametime;

public:
	void update( );
	void run( );
	void restore( );

	vec3_t velocity;
	vec3_t origin;

	float m_perfect_accuracy;
};

extern InputPrediction g_inputpred;