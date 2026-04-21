// -*-c++-*-

/*
 *Copyright:

 Cyrus2D
 Modified by Omid Amini, Nader Zare
 
 Gliders2d
 Modified by Mikhail Prokopenko, Peter Wang

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "bhv_basic_move.h"
#include "strategy.h"
#include "bhv_basic_tackle.h"
#include "neck_offensive_intercept_neck.h"
#include "bhv_basic_block.h"

#include "basic_actions/basic_actions.h"
#include "basic_actions/body_go_to_point.h"
#include "basic_actions/body_intercept.h"
#include "basic_actions/neck_turn_to_ball_or_scan.h"
#include "basic_actions/neck_scan_field.h"
#include "basic_actions/neck_turn_to_low_conf_teammate.h"

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

#include "neck_offensive_intercept_neck.h"
#include <rcsc/player/soccer_intention.h>
#include "bhv_unmark.h"

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_BasicMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_BasicMove" );

    const WorldModel & wm = agent->world();

    //-----------------------------------------------
    // tackle
    // G2d: tackle probability
    double doTackleProb = 0.8;
    if (wm.ball().pos().x < 0.0)
    {
      doTackleProb = 0.5;
    }

    if ( Bhv_BasicTackle( doTackleProb, 80.0 ).execute( agent ) )
    {
        return true;
    }

    /*--------------------------------------------------------*/
    // chase ball
    const int self_min = wm.interceptTable().selfStep();
    const int mate_min = wm.interceptTable().teammateStep();
    const int opp_min = wm.interceptTable().opponentStep();

    const Vector2D target_point = Strategy::i().getPosition( wm.self().unum() );

    // G2d: to retrieve opp team name
    // C2D: Helios 18 Tune removed -> replace with BNN
    // bool helios2018 = false;
    // if (wm.opponentTeamName().find("HELIOS2018") != std::string::npos)
	// helios2018 = true;
//    if (std::min(self_min, mate_min) < opp_min){
//
//    }else{
//        if (Bhv_BasicBlock().execute(agent)){
//            return true;
//        }
//    }
    // G2d: role
    int role = Strategy::i().roleNumber( wm.self().unum() );

    // G2D: blocking

    Vector2D ball = wm.ball().pos();

    double block_d = -10.0;

    Vector2D me = wm.self().pos();
    Vector2D homePos = target_point;
    int num = role;

    auto opps = wm.opponentsFromBall();
    const PlayerObject * nearest_opp
            = ( opps.empty()
                ? static_cast< PlayerObject * >( 0 )
                : opps.front() );
    const double nearest_opp_dist = ( nearest_opp
                                      ? nearest_opp->distFromSelf()
                                      : 1000.0 );
    if (ball.x < block_d)
    {
        double block_line = -38.0;

    //  if (helios2018)
    //      block_line = -48.0;

    // acknowledgement: fragments of Marlik-2012 code
        if( (num == 2 || num == 3) && homePos.x < block_line &&
            !( num == 2 && ball.x < -46.0 && ball.y > -18.0 && ball.y < -6.0 &&
               opp_min <= 3 && opp_min <= mate_min && ball.dist(me) < 9.5 ) &&
            !( num == 3 && ball.x < -46.0 && ball.y <  18.0 && ball.y >  6.0  &&
               opp_min <= 3 && opp_min <= mate_min && ball.dist(me) < 9.5 ) ) // do not block in this situation
        {
            // do nothing
        }
        else if( (num == 2 || num == 3) && fabs(wm.ball().pos().y) > 22.0 )
        {
            // do nothing
        }
        else if (Bhv_BasicBlock().execute(agent)){
            return true;
        }

    } // end of block


    // ============================================================
    // Bolt: 防守盯人逻辑
    // ============================================================
    if ( shouldDefendMark( agent, role, ball ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": defending mark opponent" );
        agent->debugClient().addMessage( "DefendMark" );
        return true;
    }

    // G2d: pressing
    int pressing = 13;

    if ( role >= 6 && role <= 8 && wm.ball().pos().x > -30.0 && wm.self().pos().x < 10.0 )
        pressing = 7;

    if (fabs(wm.ball().pos().y) > 22.0 && wm.ball().pos().x < 0.0 && wm.ball().pos().x > -36.5 && (role == 4 || role == 5) ) 
        pressing = 23;

    // C2D: Helios 18 Tune removed -> replace with BNN
    // if (helios2018) 
	// pressing = 4;

    if ( ! wm.kickableTeammate()
         && ( self_min <= 3
              || ( self_min <= mate_min
                   && self_min < opp_min + pressing ) // pressing
              )
         )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": intercept" );
        Body_Intercept().execute( agent );
        agent->setNeckAction( new Neck_OffensiveInterceptNeck() );

        return true;
    }



// G2D : offside trap
    double first = 0.0, second = 0.0;
    const auto t3_end = wm.teammatesFromSelf().end();
        for ( auto it = wm.teammatesFromSelf().begin();
              it != t3_end;
              ++it )
        {
            double x = (*it)->pos().x;
            if ( x < second )
            {
                second = x;
                if ( second < first )
                {
                    std::swap( first, second );
                }
            }
        }

   double buf1 = 3.5;
   double buf2 = 4.5;

   if( me.x < -37.0 && opp_min < mate_min &&
       (homePos.x > -37.5 || wm.ball().inertiaPoint(opp_min).x > -36.0 ) &&
         second + buf1 > me.x && wm.ball().pos().x > me.x + buf2)
   {
        Body_GoToPoint( rcsc::Vector2D( me.x + 15.0, me.y ),
                        0.5, ServerParam::i().maxDashPower(), // maximum dash power
                        ServerParam::i().maxDashPower(),     // preferred dash speed
                        2,                                  // preferred reach cycle
                        true,                              // save recovery
                        5.0 ).execute( agent );

        if (wm.kickableOpponent() && wm.ball().distFromSelf() < 12.0) // C2D
            agent->setNeckAction(new Neck_TurnToBall());
        else
            agent->setNeckAction(new Neck_TurnToBallOrScan(4)); // C2D
        return true;
   }

    if (std::min(self_min, mate_min) < opp_min){
        if (Bhv_Unmark().execute(agent))
            return true;
    }
    const double dash_power = Strategy::get_normal_dash_power( wm );

    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;

    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_BasicMove target=(%.1f %.1f) dist_thr=%.2f",
                  target_point.x, target_point.y,
                  dist_thr );

    agent->debugClient().addMessage( "BasicMove%.0f", dash_power );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    if ( ! Body_GoToPoint( target_point, dist_thr, dash_power
                           ).execute( agent ) )
    {
        Body_TurnToBall().execute( agent );
    }

    if ( wm.kickableOpponent()
         && wm.ball().distFromSelf() < 18.0 )
    {
        agent->setNeckAction( new Neck_TurnToBall() );
    }
    else
    {
        agent->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!
  Bolt: 判断是否需要防守盯人，并执行盯人动作（优化版本）
  只在关键时刻盯人，避免破坏整体阵型
*/
bool
Bhv_BasicMove::shouldDefendMark( PlayerAgent * agent,
                                 const int & role,
                                 const Vector2D & ball )
{
    const WorldModel & wm = agent->world();

    // 1. 必须在己方半场深处（x < -40）
    if ( wm.self().pos().x > -40.0 )
    {
        return false;
    }

    // 2. 对方必须控球
    if ( ! wm.kickableOpponent() )
    {
        return false;
    }

    // 3. 只在非常危险区域执行盯人（x < -40）
    if ( ball.x > -40.0 )
    {
        return false;
    }

    // 4. 只由后卫角色执行盯人（角色2-5）
    if ( role < 2 || role > 5 )
    {
        return false;
    }

    // 5. 找到最危险的对方球员（距离球门最近的无球球员）
    const PlayerObject * most_dangerous_opp = nullptr;
    double min_dist_to_goal = 100.0;
    Vector2D our_goal( -ServerParam::i().pitchHalfLength(), 0.0 );

    for ( const PlayerObject * opp : wm.opponentsFromSelf() )
    {
        if ( opp->posCount() > 10 )
            continue;
        // 不盯持球的对方球员（由拦截逻辑处理）
        if ( opp->distFromBall() < 2.0 )
            continue;

        double dist_to_goal = opp->pos().dist( our_goal );
        if ( dist_to_goal < min_dist_to_goal && dist_to_goal < 15.0 )
        {
            min_dist_to_goal = dist_to_goal;
            most_dangerous_opp = opp;
        }
    }

    if ( ! most_dangerous_opp )
    {
        return false;
    }

    // 6. 检查自己是否是最近的队友（避免多个后卫盯同一个对手）
    double my_dist_to_opp = wm.self().pos().dist( most_dangerous_opp->pos() );
    for ( const PlayerObject * mate : wm.teammatesFromSelf() )
    {
        if ( mate->posCount() > 10 )
            continue;
        if ( mate->goalie() )
            continue;

        double mate_dist_to_opp = mate->pos().dist( most_dangerous_opp->pos() );
        if ( mate_dist_to_opp < my_dist_to_opp - 2.0 )
        {
            // 有其他队友更近，不盯人
            return false;
        }
    }

    // 7. 计算盯防目标点（站在对方和球门之间，但保持在阵型范围内）
    Vector2D mark_point = calculateMarkPoint( wm.self().pos(),
                                               most_dangerous_opp->pos(),
                                               our_goal );

    // 8. 限制盯人位置不要偏离原站位太多（最多偏离3米）
    Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    if ( mark_point.dist( home_pos ) > 3.0 )
    {
        // 过于偏离阵型，不盯人
        return false;
    }

    // 9. 执行移动到盯人位置
    double dash_power = getDashPower( agent );
    double dist_thr = 1.0;

    Body_GoToPoint( mark_point, dist_thr, dash_power ).execute( agent );
    agent->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );

    dlog.addText( Logger::TEAM,
                  __FILE__": marking dangerous opponent at (%.2f, %.2f)",
                  mark_point.x, mark_point.y );

    return true;
}

/*-------------------------------------------------------------------*/
/*!
  Bolt: 计算盯防目标点
*/
Vector2D
Bhv_BasicMove::calculateMarkPoint( const Vector2D & defender_pos,
                                    const Vector2D & opponent_pos,
                                    const Vector2D & our_goal_pos )
{
    // 计算对方到我方球门的向量
    Vector2D opp_to_goal = our_goal_pos - opponent_pos;

    // 计算防守球员到对方球员的向量
    Vector2D defender_to_opp = opponent_pos - defender_pos;

    // 计算防守球员应该在的位置（在对方和球门之间）
    // 比例：距离球门越近，防守越靠前
    double dist_to_goal = defender_pos.dist( our_goal_pos );
    double dist_to_opp = defender_pos.dist( opponent_pos );

    double mark_ratio = dist_to_goal / ( dist_to_goal + dist_to_opp + 0.1 );

    Vector2D mark_point = opponent_pos + opp_to_goal * mark_ratio;

    // 限制 Y 坐标在合理范围内
    mark_point.y = std::max( -20.0,
                            std::min( 20.0, mark_point.y ) );

    return mark_point;
}

/*-------------------------------------------------------------------*/
/*!
  Bolt: 获取冲刺力量
*/
double
Bhv_BasicMove::getDashPower( const PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const ServerParam & SP = ServerParam::i();

    // 根据球员的体力和位置确定冲刺力量
    double stamina = wm.self().stamina();
    double max_power = SP.maxDashPower();

    // 在己方半场危险区域，使用最大力量
    if ( wm.self().pos().x < -30.0 )
    {
        return max_power;
    }

    // 在其他区域，根据体力调整力量
    double power = max_power;
    if ( stamina < 2000.0 )
    {
        power = max_power * 0.7;  // 体力不足时，降低力量
    }
    else if ( stamina < 3000.0 )
    {
        power = max_power * 0.85;
    }

    return power;
}
