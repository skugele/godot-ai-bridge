# GDScript: agent.gd

extends Node2D

const LINEAR_DELTA = 10  # change in pixels - used for linear movements
const ANGULAR_DELTA = 5.0  # change in degrees - used for rotational actions

# an identifier for this agent
export var id = 1

# a list of actions that are pending execution
onready var pending_actions = []


func _process(_delta):
	var action = pending_actions.pop_front()
	if action:
		execute(action)

func add_action(action):
	pending_actions.push_back(action)
		
func execute(action):
	match action:
		'up': global_position.y -= LINEAR_DELTA
		'down': global_position.y += LINEAR_DELTA
		'left': global_position.x -= LINEAR_DELTA
		'right': global_position.x += LINEAR_DELTA
		'rotate_clockwise': rotation_degrees += ANGULAR_DELTA
		'rotate_counterclockwise': rotation_degrees -= ANGULAR_DELTA
		
		# default case: unrecognized actions
		_: print('unrecogized action: ', action) 
