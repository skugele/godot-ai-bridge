# GDScript: world.gd

extends Node2D

# Godot AI Bridge (GAB) Variables
onready var gab = $GabLib # library reference
onready var context = null # socket context (a unique identifier using in call to "send")

onready var pub_options = {'port':10001} # state publisher options
onready var sub_options = {'port':10002} # action listener options

func _ready():
	
	# Initializes GAB's state publisher.
	context = gab.connect(pub_options)
	
	# Initializes GAB's action listener.
	gab.start_listener(sub_options)

func _process(_delta):
	for agent in $Agents.get_children():
		_publish_state(agent)

### publishes the agent's current state to all clients ###
func _publish_state(agent):
	
	# topic identifier for this message (can be used as a message filter by a recipient).
	var topic = '/demo/agent/%s' % agent.id
	
	# message payload to be broadcast by the state publisher
	var msg = {
		'position' : [agent.global_position.x, agent.global_position.y],
		'rotation_in_degrees' : agent.rotation_degrees
	}

	gab.send(msg, context, topic)

### signal handler for the "action_received" signal, which is emitted by GAB when an action request is received ###
func _on_action_received(action_details):
	print('action received ----> "%s"' % action_details)

	for agent in $Agents.get_children():
		if action_details['agent_id'] == agent.id:
			agent.add_action(action_details['action'])
