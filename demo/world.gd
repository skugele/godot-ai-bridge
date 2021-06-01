extends Node2D

onready var agent = $Agent

# Godot AI Bridge (GAB) Variables
onready var gab = $GabLib # library node reference
onready var context = null # socket context (a unique identifier using in call to "send")

onready var pub_options = {'port':10001} # state publisher options
onready var sub_options = {'port':10002} # action listener options

func _ready():
	# Establishes a socket for sending outgoing state to clients. The returned context must be used when calling the 
	# library's send method.
	context = gab.connect(pub_options)
	
	# Starts a listener for receiving incoming actions from clients. The library raises the "action_received"
	# signal when a new action is received.
	gab.start_listener(sub_options)

func _process(delta):
	_publish_state()

# publishes the agent's current state to all clients
func _publish_state():
	var topic = '/agent/1'
	var msg = {
		'position' : [agent.global_position.x, agent.global_position.y],
		'rotation_in_degrees' : agent.rotation_degrees
	}

	gab.send(msg, context, topic)

# signal handler for incoming actions
func _on_action_received(action_details):
	agent.add_action(action_details['data']['action'])
