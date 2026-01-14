#!/usr/bin/env python3

import sys
import requests
from PIL import Image
from io import BytesIO
import os
import time
from datetime import datetime
import struct

import argparse

parser=argparse.ArgumentParser()

parser.add_argument("--url", help="media image url")
parser.add_argument("--artist", help="artist name")
parser.add_argument("--album", help="album name")
parser.add_argument("--title", help="media title")

args=parser.parse_args()

url = args.url
media_artist = args.artist
media_album = args.album
media_title = args.title

# Setup logging
log_file = '/config/www/media/album_art/_resize_debug.log'

def log(message):
    timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    with open(log_file, 'a') as f:
        f.write(f"[{timestamp}] {message}\n")
    print(message)

# Log all parameters
log("=" * 60)
log(f"URL: {url}")
log(f"Artist: {media_artist}")
log(f"Album: {media_album}")
log(f"Title: {media_title}")
log(len(sys.argv))
log(sys.argv)

try:
    # Create cache key from artist + album (or title if no album)
    if media_album and media_album.lower() not in ['unknown', 'none', '', 'null']:
        cache_key = f"{media_artist}_{media_album}".lower()
        log(f"Using artist+album for cache key")
    else:
        # No album name, use artist + title instead
        cache_key = f"{media_artist}_{media_title}".lower()
        log(f"No album name, using artist+title for cache key")
    
    log(f"Cache key (before sanitize): {cache_key}")
    
    # Remove special characters
    cache_key = ''.join(c if c.isalnum() else '_' for c in cache_key)
    
    log(f"Cache key (after sanitize): {cache_key}")
    
    # Setup cache directory
    cache_dir = '/config/www/media/album_art'
    os.makedirs(cache_dir, exist_ok=True)
    
    cache_file_rgb565 = f"{cache_dir}/{cache_key}.rgb565"
    log(f"Cache file path: {cache_file_rgb565}")
    
    # Check if cached version exists
    if os.path.exists(cache_file_rgb565):
        log(f"Using cached album art: {cache_key}")
        file_size = os.path.getsize(cache_file_rgb565)
        log(f"Cached RGB565 file size: {file_size} bytes")
    else:
        # Download with retry logic
        max_retries = 3
        retry_delay = 1  # seconds
        
        for attempt in range(max_retries):
            try:
                log(f"Download attempt {attempt + 1}/{max_retries} from: {url}")
                response = requests.get(url, timeout=10)
                response.raise_for_status()
                log(f"Downloaded {len(response.content)} bytes")
                break  # Success, exit retry loop
                
            except requests.exceptions.HTTPError as e:
                if response.status_code in [500, 502, 503, 504]:
                    # Server error - retry
                    if attempt < max_retries - 1:
                        log(f"Server error {response.status_code}, retrying in {retry_delay}s...")
                        time.sleep(retry_delay)
                        retry_delay *= 2  # Exponential backoff
                        continue
                    else:
                        log(f"Failed after {max_retries} attempts")
                        raise
                else:
                    # Other HTTP error - don't retry
                    raise
                    
            except (requests.exceptions.Timeout, requests.exceptions.ConnectionError) as e:
                # Network error - retry
                if attempt < max_retries - 1:
                    log(f"Network error: {str(e)}, retrying in {retry_delay}s...")
                    time.sleep(retry_delay)
                    retry_delay *= 2
                    continue
                else:
                    log(f"Failed after {max_retries} attempts")
                    raise
        
        # Open and resize the image
        img = Image.open(BytesIO(response.content))
        log(f"Original size: {img.size}, mode: {img.mode}")
        
        # Convert to RGB (no alpha channel)
        img = img.convert('RGB')
        
        # Resize with high-quality resampling
        img = img.resize((110, 110), Image.Resampling.LANCZOS)
        log(f"Resized to: {img.size}")
        
        # Reduce color palette to 256 colors for smaller file size
        img_reduced = img.quantize(colors=256, method=2)
        img_reduced = img_reduced.convert('RGB')
        log(f"Reduced to 256 colors")
        
        # Save RGB565 raw format for LVGL (Big-Endian)
        pixels = []
        for y in range(110):
            for x in range(110):
                r, g, b = img_reduced.getpixel((x, y))
                # Convert RGB888 to RGB565
                # R: 8 bits -> 5 bits (>> 3)
                # G: 8 bits -> 6 bits (>> 2)
                # B: 8 bits -> 5 bits (>> 3)
                rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
                # Pack as BIG-ENDIAN 16-bit unsigned integer (>H not <H)
                pixels.append(struct.pack('>H', rgb565))
        
        with open(cache_file_rgb565, 'wb') as f:
            f.write(b''.join(pixels))
        
        rgb565_size = os.path.getsize(cache_file_rgb565)
        log(f"Saved RGB565: {rgb565_size} bytes (should be 24200 bytes for 110x110)")
        
        if rgb565_size != 24200:
            log(f"WARNING: RGB565 file size is incorrect! Expected 24200 bytes, got {rgb565_size}")
    
    # Output the cache key for Home Assistant to read
    print(f"CACHE_KEY:{cache_key}")
    log(f"SUCCESS: {cache_key}")
    
except Exception as e:
    error_msg = f"ERROR: {str(e)}"
    log(error_msg)
    print(error_msg, file=sys.stderr)
    sys.exit(1)
