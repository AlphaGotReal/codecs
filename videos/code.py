import yt_dlp

def download_youtube_media(url):
    ydl_opts = {
        'format': 'bestvideo[height<=2160][fps<=60]+bestaudio/best',
        'merge_output_format': 'mp4',
        'outtmpl': 'video%(playlist_index)s.%(ext)s',
        'ignoreerrors': True,
        'quiet': False 
    }

    try:
        with yt_dlp.YoutubeDL(ydl_opts) as ydl:
            print(f"Starting download process for: {url}...")
            ydl.download([url])
            print("\nDownload process finished successfully!")
    except Exception as e:
        print(f"\nAn error occurred: {e}")

if __name__ == "__main__":
    target_link = "https://youtu.be/8Ns3murXFhw?list=PLDjATtZ3vCth5pEB3XIEZSdwPDV61OvDL" 
    download_youtube_media(target_link)
