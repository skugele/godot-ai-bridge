# GDScript: agent.gd

extends Node2D

const LINEAR_DELTA = 10  # change in pixels - used for linear movements
const ANGULAR_DELTA = 5.0  # change in degrees - used for rotational actions

# an identifier for this agent
export var id = 1

# a queue of actions that are pending execution
onready var pending_actions = []


func _process(_delta):
	
	# removes and executes the oldest pending action from the queue (if one exists)
	var action = pending_actions.pop_front()
	if action:
		execute(action)

# adds an action to the agent's pending_actions queue for later execution
func add_action(action):
	pending_actions.push_back(action)

# executes an action
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


func get_state():
	return {
		'position' : [global_position.x, global_position.y],
		'rotation_in_degrees' : rotation_degrees
	}
