"""
Bragi Favorite Sync
AppDaemon App

Installation:
    1. Add music-assistant-client to the AppDaemon addon's python libraries config
    2. Place this file in: /config/appdaemon/apps/bragi_sync.py
    3. Add configuration to /config/appdaemon/apps/apps.yaml:

bragi_sync:
    module: bragi_sync
    class: BragiSync
    ma_url: "http://music-assistant.local:8095"
    ma_token: "YOUR_LONG_LIVED_TOKEN"
    player_id: "player-id"
    input_boolean: "input_boolean.current_track_is_favorite"

Get your Music Assistant token:
    Settings -> Profile -> Generate Long-Lived Token
"""

import appdaemon.plugins.hass.hassapi as hass
from music_assistant_client import MusicAssistantClient
from music_assistant_models.enums import EventType
from typing import Optional
import asyncio


class BragiSync(hass.Hass):

    # Init
    async def initialize(self):
        self.ma_url = self.args.get("ma_url")
        self.ma_token = self.args.get("ma_token")
        self.player_id = self.args.get("player_id")
        self.input_boolean = self.args.get("input_boolean")

        self.ma_client = None
        self.last_favorite_state = None
        self.connection_task = None
        self.reconnect_delay = 5

        # Validate configuration
        if not all([self.ma_url, self.ma_token, self.player_id, self.input_boolean]):
            self.error("Missing required configuration parameters")
            return

        # Start connection in background
        self.connection_task = asyncio.create_task(self.connect_ma())

        self.log(f"Bragi Favorite Sync initialized for player {self.player_id}")

    # Connect to Music Assistant
    async def connect_ma(self):
        while True:
            try:
                self.log("Connecting to Music Assistant...")

                async with MusicAssistantClient(
                    self.ma_url, None, token=self.ma_token
                ) as client:
                    self.ma_client = client

                    # Subscribe to media item updates (for favorite changes)
                    def media_callback(event):
                        asyncio.create_task(self.on_media_item_update(event))

                    client.subscribe(media_callback, EventType.MEDIA_ITEM_UPDATED)

                    def player_callback(event):
                        asyncio.create_task(self.on_player_update(event))

                    client.subscribe(player_callback, EventType.PLAYER_UPDATED)

                    self.log(
                        "Connected to Music Assistant! Monitoring player updates and favorite changes..."
                    )

                    # Start listening (blocks until disconnected)
                    await client.start_listening()

            except asyncio.CancelledError:
                self.log("Connection cancelled")
                break
            except Exception as e:
                self.error(f"Connection error: {e}")
                self.log(f"Retry: {self.reconnect_delay}s")
                await asyncio.sleep(self.reconnect_delay)

    # Media Item event handler
    async def on_media_item_update(self, event_msg):
        try:
            if not event_msg.object_id:
                return

            is_favorite = event_msg.data.get("favorite", False)
            await self.set_favorite_state(is_favorite)

        except Exception as e:
            self.error(f"Error media_item update: {e}")

    # Player event handler
    async def on_player_update(self, event_msg):
        try:
            if event_msg.object_id != self.player_id:
                return

            current_media = event_msg.data.get("current_media")
            current_media_uri = current_media.get("uri")
            await self.check_favorite_status(current_media_uri)

        except Exception as e:
            self.error(f"Error with player update: {e}")

    # Set favorite state for input_boolean in Home Assistant
    async def set_favorite_state(self, state: bool):
        try:
            service = "turn_on" if state else "turn_off"
            await self.call_service(f"input_boolean/{service}", entity_id=self.input_boolean)
            self.last_favorite_state = state

        except Exception as e:
            self.error(f"Error updating HA: {e}")

    # Check Music Assistant track's favorite status
    async def check_favorite_status(self, uri: str):
        try:
            parts = uri.replace("://", "/").split("/")
            if len(parts) < 3:
                self.warning(f"Invalid URI: {uri}")
                return

            provider = parts[0]
            media_type = parts[1]
            item_id = parts[2]

            item = await self.ma_client.music.get_item(
                media_type=media_type,
                item_id=item_id,
                provider_instance_id_or_domain=provider
            )

            is_favorite = getattr(item, 'favorite', False)

            # Update HA if changed
            if is_favorite != self.last_favorite_state:
                await self.set_favorite_state(is_favorite)
                # self.log(f"favorite: {is_favorite}")

        except Exception as e:
            self.error(f"Error checking favorite status: {str(e)}")

    # Doooooooooooooooooooooooooooooooooooom
    async def terminate(self):
        self.log("Shutting down Bragi Sync...")

        if self.connection_task:
            self.connection_task.cancel()
            try:
                await self.connection_task
            except asyncio.CancelledError:
                pass

        if self.ma_client:
            try:
                await self.ma_client.disconnect()
            except:
                pass

        self.log("Bragi Sync terminated")
