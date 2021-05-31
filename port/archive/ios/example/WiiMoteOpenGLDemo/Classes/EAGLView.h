//
//  EAGLView.h
//  AppleCoder-OpenGLES-00
//
//  Created by Simon Maurice on 18/03/09.
//  Copyright Simon Maurice 2009. All rights reserved.
//


#import <UIKit/UIKit.h>
#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES1/gl.h>
#import <OpenGLES/ES1/glext.h>

/*
This class wraps the CAEAGLLayer from CoreAnimation into a convenient UIView subclass.
The view content is basically an EAGL surface you render your OpenGL scene into.
Note that setting the view non-opaque will only work if the EAGL surface has an alpha channel.
*/
@interface EAGLView : UIView {
    
@private
    /* The pixel dimensions of the backbuffer */
    GLint backingWidth;
    GLint backingHeight;
    
    EAGLContext *context;
    
    /* OpenGL names for the renderbuffer and framebuffers used to render to this view */
    GLuint viewRenderbuffer, viewFramebuffer;
    
    /* OpenGL name for the depth buffer that is attached to viewFramebuffer, if it exists (0 if it does not exist) */
    GLuint depthRenderbuffer;
    
    NSTimer *animationTimer;
    NSTimeInterval animationInterval;
	
	GLuint textures[1];
	GLfloat rota;

	BOOL useRotationMatrix;
	
	// use rotation matrix
	GLfloat rotationMatrix[16];
	
	// use euler angles
	int rotateX;
	int rotateY;
	int rotateZ;
}

@property NSTimeInterval animationInterval;
@property BOOL useRotationMatrix;

- (void)startAnimation;
- (void)stopAnimation;
- (void)drawView;

- (void)setupView;
- (void)checkGLError:(BOOL)visibleCheck;

- (void)setRotationMatrix:(float[4][4]) matrix;
- (void)setRotationX:(int)X Y:(int)Y Z:(int)Z;
- (void)loadTexture;

@end
