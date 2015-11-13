//
//  EAGLView.m
//  AppleCoder-OpenGLES-00
//
//  Created by Simon Maurice on 18/03/09.
//  Copyright Simon Maurice 2009. All rights reserved.
//



#import <QuartzCore/QuartzCore.h>
#import <OpenGLES/EAGLDrawable.h>

#import "EAGLView.h"
#import "WiiMoteOpenGLDemoAppDelegate.h"

#define USE_DEPTH_BUFFER 1
#define DEGREES_TO_RADIANS(__ANGLE) ((__ANGLE) / 180.0 * M_PI)

#define USE_BLUETOOTH

// A class extension to declare private methods
@interface EAGLView ()

@property (nonatomic, retain) EAGLContext *context;
@property (nonatomic, assign) NSTimer *animationTimer;

- (BOOL) createFramebuffer;
- (void) destroyFramebuffer;

@end


@implementation EAGLView

@synthesize context;
@synthesize animationTimer;
@synthesize animationInterval;
@synthesize useRotationMatrix;

// You must implement this method
+ (Class)layerClass {
    return [CAEAGLLayer class];
}

- (id)initWithFrame:(CGRect)frame {
	if ((self = [super initWithFrame:frame])) {
        // Get the layer
        CAEAGLLayer *eaglLayer = (CAEAGLLayer *)self.layer;
        
        eaglLayer.opaque = YES;
        eaglLayer.drawableProperties = [NSDictionary dictionaryWithObjectsAndKeys:
                                        [NSNumber numberWithBool:NO], kEAGLDrawablePropertyRetainedBacking, kEAGLColorFormatRGBA8,
										kEAGLDrawablePropertyColorFormat, nil];
        
        context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES1];
        
        if (!context || ![EAGLContext setCurrentContext:context]) {
            [self release];
            return nil;
        }
        
        animationInterval = 1.0 / 60.0;
		rota = 0.0;
		
		[self setupView];
		[self loadTexture];
		
	}
	return self;
}

- (void)drawView {
	
	// Our new object definition code goes here
    const GLfloat cubeVertices[] = {
        
        // Define the front face
        -0.24415584, 1.0, 0.207792208,             // top left
        -0.24415584, -1.0, 0.207792208,            // bottom left
        0.24415584, -1.0, 0.207792208,             // bottom right
        0.24415584, 1.0, 0.207792208,              // top right
        
        // Top face
        -0.24415584, 1.0, -0.207792208,            // top left (at rear)
        -0.24415584, 1.0, 0.207792208,             // bottom left (at front)
        0.24415584, 1.0, 0.207792208,              // bottom right (at front)
        0.24415584, 1.0, -0.207792208,             // top right (at rear)
        
        // Rear face
        0.24415584, 1.0, -0.207792208,             // top right (when viewed from front)
        0.24415584, -1.0, -0.207792208,            // bottom right
        -0.24415584, -1.0, -0.207792208,           // bottom left
        -0.24415584, 1.0, -0.207792208,            // top left
        
        // bottom face
        -0.24415584, -1.0, 0.207792208,
        -0.24415584, -1.0, -0.207792208,
        0.24415584, -1.0, -0.207792208,
        0.24415584, -1.0, 0.207792208,
        
        // left face
        -0.24415584, 1.0, -0.207792208,
        -0.24415584, 1.0, 0.207792208,
        -0.24415584, -1.0, 0.207792208,
        -0.24415584, -1.0, -0.207792208,
        
        // right face
        0.24415584, 1.0, 0.207792208,
        0.24415584, 1.0, -0.207792208,
        0.24415584, -1.0, -0.207792208,
        0.24415584, -1.0, 0.207792208
    };
	
    const GLfloat squareTextureCoords[] = {
		
        // Front face - WiiTop
        0, 0,       // top left
        0, 0.749023438,       // bottom left
        0.18359375, 0.749023438,       // bottom right
        0.18359375, 0,       // top right
		
        // Top face - WiiRear
        0.25, 1,       // top left
        0.25, 0.844726563,       // bottom left
        0.43359375, 0.844726563,       // bottom right
        0.43359375, 1,       // top right
        
        // Rear face
        0.25, 0,       // bottom right
        0.25, 0.751953125,       // top right
        0.43359375, 0.751953125,       // top left
        0.43359375, 0,       // bottom left
        
        // Bottom face
        0, 0.84375,       // top left
        0, 1,       // bottom left
        0.18359375, 1,       // bottom right
        0.18359375, 0.84375,       // top right
        
        // Left face
        0.5,       0,     // bottom left
        0.6640625, 0,		// bottom right
        0.6640625, 0.751953125,				// top right
        0.5,       0.751953125,				// top left
        
        // Right face
        0.75, 0,       // bottom left
        0.9140625, 0,       // bottom right
        0.9140625, 0.751953125,       // top right
        0.75, 0.751953125,       // top left
    };
	
    [EAGLContext setCurrentContext:context];    
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, viewFramebuffer);
    glViewport(0, 0, backingWidth, backingHeight);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	
	glMatrixMode(GL_MODELVIEW);
	
	// Our new drawing code goes here
	glLoadIdentity();
    glTranslatef(0.0, 0.0, -2.0);
	
#ifdef USE_BLUETOOTH
	if (self.useRotationMatrix) {
		glMultMatrixf(rotationMatrix);
	}
	glRotatef(rotateX, 1.0f, 0.0f, 0.0f);
	glRotatef(rotateY, 0.0f, 1.0f, 0.0f);
	glRotatef(rotateZ, 0.0f, 0.0f, 1.0f);
#else
	rota += 1;
	glRotatef(rota, 0.0, 0.5, 0.0);
	glRotatef(rota, 0.0, 0.0, 1.0);
#endif
	
	glVertexPointer(3, GL_FLOAT, 0, cubeVertices);
    glEnableClientState(GL_VERTEX_ARRAY);

    glTexCoordPointer(2, GL_FLOAT, 0, squareTextureCoords);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	
	glColor4f(1.0, 1.0, 1.0, 1.0);

	// Draw the front face in Red
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	
	// Draw the top face in green
	glDrawArrays(GL_TRIANGLE_FAN, 4, 4);
	
	// Draw the rear face in Blue
	glDrawArrays(GL_TRIANGLE_FAN, 8, 4);
	
	// Draw the bottom face
	glDrawArrays(GL_TRIANGLE_FAN, 12, 4);

	// Draw the left face
	glDrawArrays(GL_TRIANGLE_FAN, 16, 4);
	
	// Draw the right face
	glDrawArrays(GL_TRIANGLE_FAN, 20, 4);
	
	
    glBindRenderbufferOES(GL_RENDERBUFFER_OES, viewRenderbuffer);
    [context presentRenderbuffer:GL_RENDERBUFFER_OES];
	[self checkGLError:NO];
}

- (void)layoutSubviews {
    [EAGLContext setCurrentContext:context];
    [self destroyFramebuffer];
    [self createFramebuffer];
    [self drawView];
}


- (BOOL)createFramebuffer {
    
    glGenFramebuffersOES(1, &viewFramebuffer);
    glGenRenderbuffersOES(1, &viewRenderbuffer);
    
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, viewFramebuffer);
    glBindRenderbufferOES(GL_RENDERBUFFER_OES, viewRenderbuffer);
    [context renderbufferStorage:GL_RENDERBUFFER_OES fromDrawable:(CAEAGLLayer*)self.layer];
    glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_RENDERBUFFER_OES, viewRenderbuffer);
    
    glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_WIDTH_OES, &backingWidth);
    glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_HEIGHT_OES, &backingHeight);
    
    if (USE_DEPTH_BUFFER) {
        glGenRenderbuffersOES(1, &depthRenderbuffer);
        glBindRenderbufferOES(GL_RENDERBUFFER_OES, depthRenderbuffer);
        glRenderbufferStorageOES(GL_RENDERBUFFER_OES, GL_DEPTH_COMPONENT16_OES, backingWidth, backingHeight);
        glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_DEPTH_ATTACHMENT_OES, GL_RENDERBUFFER_OES, depthRenderbuffer);
    }
    
    if(glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES) != GL_FRAMEBUFFER_COMPLETE_OES) {
        NSLog(@"failed to make complete framebuffer object %x", glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES));
        return NO;
    }
    
    return YES;
}

- (void)setupView {
	
    const GLfloat zNear = 0.1, zFar = 1000.0, fieldOfView = 60.0;
    GLfloat size;
	
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    size = zNear * tanf(DEGREES_TO_RADIANS(fieldOfView) / 2.0);
	
	// This give us the size of the iPhone display
    CGRect rect = self.bounds;
    glFrustumf(-size, size, -size / (rect.size.width / rect.size.height), size / (rect.size.width / rect.size.height), zNear, zFar);
    glViewport(0, 0, rect.size.width, rect.size.height);
	
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

- (void)loadTexture {
    CGImageRef textureImage = [UIImage imageNamed:@"wiimote_texture.png"].CGImage;
    if (textureImage == nil) {
        NSLog(@"Failed to load texture image");
		return;
    }
	
    NSInteger texWidth = CGImageGetWidth(textureImage);
    NSInteger texHeight = CGImageGetHeight(textureImage);
	
	GLubyte *textureData = (GLubyte *)malloc(texWidth * texHeight * 4);
	
    CGContextRef textureContext = CGBitmapContextCreate(textureData,
                                                         texWidth, texHeight,
                                                         8, texWidth * 4,
                                                         CGImageGetColorSpace(textureImage),
                                                         (CGBitmapInfo)kCGImageAlphaPremultipliedLast);
	CGContextDrawImage(textureContext, CGRectMake(0.0, 0.0, (float)texWidth, (float)texHeight), textureImage);
	CGContextRelease(textureContext);
	
	glGenTextures(1, &textures[0]);
	glBindTexture(GL_TEXTURE_2D, textures[0]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texWidth, texHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, textureData);
	
	free(textureData);
	
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glEnable(GL_TEXTURE_2D);
}

- (void)destroyFramebuffer {
    
    glDeleteFramebuffersOES(1, &viewFramebuffer);
    viewFramebuffer = 0;
    glDeleteRenderbuffersOES(1, &viewRenderbuffer);
    viewRenderbuffer = 0;
    
    if(depthRenderbuffer) {
        glDeleteRenderbuffersOES(1, &depthRenderbuffer);
        depthRenderbuffer = 0;
    }
}


- (void)startAnimation {
    self.animationTimer = [NSTimer scheduledTimerWithTimeInterval:animationInterval target:self selector:@selector(drawView) userInfo:nil repeats:YES];
}


- (void)stopAnimation {
    self.animationTimer = nil;
}


- (void)setAnimationTimer:(NSTimer *)newTimer {
    [animationTimer invalidate];
    animationTimer = newTimer;
}

- (void)checkGLError:(BOOL)visibleCheck {
    GLenum error = glGetError();
    
    switch (error) {
        case GL_INVALID_ENUM:
            NSLog(@"GL Error: Enum argument is out of range");
            break;
        case GL_INVALID_VALUE:
            NSLog(@"GL Error: Numeric value is out of range");
            break;
        case GL_INVALID_OPERATION:
            NSLog(@"GL Error: Operation illegal in current state");
            break;
        case GL_STACK_OVERFLOW:
            NSLog(@"GL Error: Command would cause a stack overflow");
            break;
        case GL_STACK_UNDERFLOW:
            NSLog(@"GL Error: Command would cause a stack underflow");
            break;
        case GL_OUT_OF_MEMORY:
            NSLog(@"GL Error: Not enough memory to execute command");
            break;
        case GL_NO_ERROR:
            if (visibleCheck) {
                NSLog(@"No GL Error");
            }
            break;
        default:
            NSLog(@"Unknown GL Error");
            break;
    }
}

- (void)setRotationMatrix:(float[4][4]) matrix{
	useRotationMatrix = YES;
	// copy and linearize matrix
	int i,j;
	int pos = 0;
	for (i=0;i<4;i++)
		for (j=0; j<4; j++)
			rotationMatrix[pos++] = matrix[i][j];
}

- (void)setRotationX:(int)x Y:(int)y Z:(int)z{

	// NSLog(@"BT data: %u %u %u", x , y ,z);
	rotateX = x;
	rotateY = y;
	rotateZ = z;
}

- (void)dealloc {
    
    [self stopAnimation];
    
    if ([EAGLContext currentContext] == context) {
        [EAGLContext setCurrentContext:nil];
    }
    
    [context release];  
    [super dealloc];
}

@end
