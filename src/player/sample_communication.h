// -*-c++-*-

/*!
  \file sample_communication.h
  \brief sample communication planner Header File
*/

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA

 This code is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3, or (at your option)
 any later version.

 This code is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this code; see the file COPYING.  If not, write to
 the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

 *EndCopyright:
 */

/////////////////////////////////////////////////////////////////////

#ifndef SAMPLE_COMMUNICATION_H
#define SAMPLE_COMMUNICATION_H

#include "communication.h"

#include <rcsc/game_time.h>
#include <rcsc/types.h>
#include <rcsc/geom/vector_2d.h>  // Bolt: Add Vector2D header

namespace rcsc {
class WorldModel;
}

class SampleCommunication
    : public Communication {
private:

    int M_current_sender_unum;
    int M_next_sender_unum;
    rcsc::GameTime M_ball_send_time;
    rcsc::GameTime M_teammate_send_time[12];
    rcsc::GameTime M_opponent_send_time[12];

public:

    SampleCommunication();
    ~SampleCommunication();

    virtual
    bool execute( rcsc::PlayerAgent * agent );

    int currentSenderUnum() const { return M_current_sender_unum; }

    int nextSenderUnum() const { return M_next_sender_unum; }

private:
    void updateCurrentSender( const rcsc::PlayerAgent * agent );

    void updatePlayerSendTime( const rcsc::WorldModel & wm,
                               const rcsc::SideID side,
                               const int unum );

    bool shouldSayBall( const rcsc::PlayerAgent * agent );
    bool shouldSayOpponentGoalie( const rcsc::PlayerAgent * agent );
    bool goalieSaySituation( const rcsc::PlayerAgent * agent );

    bool sayBallAndPlayers( rcsc::PlayerAgent * agent );
    bool sayBall( rcsc::PlayerAgent * agent );
    bool sayPlayers( rcsc::PlayerAgent * agent );
    bool sayGoalie( rcsc::PlayerAgent * agent );

    bool sayIntercept( rcsc::PlayerAgent * agent );
    bool sayOffsideLine( rcsc::PlayerAgent * agent );
    bool sayDefenseLine( rcsc::PlayerAgent * agent );

    bool sayOpponents( rcsc::PlayerAgent * agent );
    bool sayTwoOpponents( rcsc::PlayerAgent * agent );
    bool sayThreeOpponents( rcsc::PlayerAgent * agent );

    bool saySelf( rcsc::PlayerAgent * agent );

    bool sayStamina( rcsc::PlayerAgent * agent );
    bool sayRecovery( rcsc::PlayerAgent * agent );

    void attentiontoSomeone( rcsc::PlayerAgent * agent );

    // Bolt: RL-specific communication methods
    bool sayRLValue( rcsc::PlayerAgent * agent, double value_estimate );
    bool sayRLIntention( rcsc::PlayerAgent * agent, int action_index, const rcsc::Vector2D & target );
    bool sayRLCoordination( rcsc::PlayerAgent * agent );

    // Parse RL messages from teammates
    void parseRLMessages( const rcsc::WorldModel & wm );

    // Get aggregated RL signals from teammates
    double getTeamValueEstimate() const;
    int getTeamBestAction() const;
};

#endif
