package
{
	import flash.display.DisplayObject;
	import flash.display.MovieClip;
	import flash.display.Shape;
	import flash.display.Sprite;
	import flash.display.StageAlign;
	import flash.display.StageScaleMode;
	import flash.events.Event;
	import flash.events.ProgressEvent;
	import flash.text.TextField;
	import flash.text.TextFormat;
	import flash.text.TextFormatAlign;
	import flash.utils.getDefinitionByName;
	
	public class Preloader extends MovieClip {
		private var _progressBar		:Sprite;
		private var _progressText	:TextField;
		private var _infoText		:TextField;
		
		private static const PROGRESS_BAR_LENGTH:Number = 300;
		private static const PROGRESS_BAR_HEIGHT:Number = 10;
		
		public function Preloader() {
			
			//Entire application visible in the specified area without distortion while maintaining the original aspect ratio of the application.
			stage.scaleMode = StageScaleMode.SHOW_ALL;

			addEventListener(Event.ENTER_FRAME, checkFrame);
			loaderInfo.addEventListener(ProgressEvent.PROGRESS, progress);
			// show loader
			
			_progressBar = new Sprite();
			addChild(_progressBar);
			
			_infoText = new TextField();
			_infoText.selectable = false;
			_infoText.width = PROGRESS_BAR_LENGTH;
			var defaultTextFormat:TextFormat = new TextFormat("_sans", 16, 0xFFFFFF, true);
			defaultTextFormat.align = TextFormatAlign.CENTER;
			_infoText.defaultTextFormat = defaultTextFormat;		
			_infoText.text = "Loading\n\qbismSuper8 flashQuake engine";
			_infoText.y = 200;
			addChild(_infoText);
			
			_progressText = new TextField();
			_progressText.selectable = false;
			_progressText.width = _infoText.width;
			_progressText.defaultTextFormat = defaultTextFormat;
			addChild(_progressText);
			
			stage.addEventListener(Event.RESIZE, onResize);
			onResize();
		}
		
		private function onResize(e:Event = null):void
		{
			if (_progressBar)
			{
				_progressBar.x = stage.stageWidth / 2;
				_progressBar.y = stage.stageHeight / 2;
			}
			if (_progressText)
			{
				_infoText.x = (stage.stageWidth - _infoText.width) / 2;
				_infoText.y = stage.stageHeight / 2 - _infoText.textHeight - PROGRESS_BAR_HEIGHT;
				
				_progressText.x = (stage.stageWidth - _progressText.width) / 2;
				_progressText.y = stage.stageHeight / 2 + _progressText.textHeight + PROGRESS_BAR_HEIGHT;
			}
		}
		
		private function progress(e:ProgressEvent):void
		{
			if (_progressText)
			{
				_progressText.text = new Number(e.bytesLoaded / 1024 / 1024).toFixed(2) + " / " + 
					new Number(e.bytesTotal / 1024 / 1024).toFixed(2) + " MB";
					
				var amountComplete:Number = e.bytesLoaded / e.bytesTotal;
				
				_progressBar.graphics.clear();
				//Set the progress bar color from red to green				
				_progressBar.graphics.beginFill(((255 * (1 - amountComplete)) << 16) + ((255 * amountComplete) << 8));				
				_progressBar.graphics.drawRect( -PROGRESS_BAR_LENGTH / 2, -PROGRESS_BAR_HEIGHT / 2, 
					PROGRESS_BAR_LENGTH*amountComplete, PROGRESS_BAR_HEIGHT);
				_progressBar.graphics.endFill();
			}
		}
		
		private function checkFrame(e:Event):void
		{
			if (currentFrame == totalFrames)
			{
				removeEventListener(Event.ENTER_FRAME, checkFrame);
				startup();
			}
		}
		
		private function startup():void
		{
			// hide loader
			stop();
			removeChild(_progressBar);
			removeChild(_progressText);
			removeChild(_infoText);
			_progressBar = null;
			_progressText = null;
			_infoText = null;
			loaderInfo.removeEventListener(ProgressEvent.PROGRESS, progress);
			var mainClass:Class = getDefinitionByName("Main") as Class;
			addChildAt(new mainClass() as DisplayObject, 0);
		}
	}
}