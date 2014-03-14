//
// All code related to getting/setting exif orientation from:
//  http://jpegclub.org/exif_orientation.html
//
// This code has no license associated with it, but based on the description at the above
//   link, I am assuming it is released under an open source license compatible with BSD
//   If my assumptions are incorrect and you are the owner of the code, please contact me.
//

/*
 * jpegexiforient.c
 *
 * This is a utility program to get and set the Exif Orientation Tag.
 * It can be used together with jpegtran in scripts for automatic
 * orientation correction of digital camera pictures.
 *
 * The Exif orientation value gives the orientation of the camera
 * relative to the scene when the image was captured.  The relation
 * of the '0th row' and '0th column' to visual position is shown as
 * below.
 *
 * Value | 0th Row     | 0th Column
 * ------+-------------+-----------
 *   1   | top         | left side
 *   2   | top         | right side
 *   3   | bottom      | right side
 *   4   | bottom      | left side
 *   5   | left side   | top
 *   6   | right side  | top
 *   7   | right side  | bottom
 *   8   | left side   | bottom
 *
 * For convenience, here is what the letter F would look like if it were
 * tagged correctly and displayed by a program that ignores the orientation
 * tag:
 *
 *   1        2       3      4         5            6           7          8
 *
 * 888888  888888      88  88      8888888888  88                  88  8888888888
 * 88          88      88  88      88  88      88  88          88  88      88  88
 * 8888      8888    8888  8888    88          8888888888  8888888888          88
 * 88          88      88  88
 * 88          88  888888  888888
 *
 */

#define __CANT_OPEN -1
#define __PREMATURE_END -2
#define __INVALID_EXIF_DATA -3
#define __EOF 0xFF990011

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cstring>
#include <node.h>
#include <v8.h>

using namespace v8;
int get_set_orientation(int, char*);

struct Orientation_req
{
  int result;
  int orientation;
  char *filename;
  Persistent<Function> callback;
};

void OrientationWorker(uv_work_t* req)
{
  Orientation_req* request = (Orientation_req*)req->data;
  request->result = get_set_orientation(request->orientation, request->filename);
}

void OrientationAfter(uv_work_t* req)
{
  HandleScope scope;
  Orientation_req* request = (Orientation_req*)req->data;
  delete req;

  Handle<Value> argv[2];

  if (request->result < 0) {
    argv[1] = Undefined();
    switch (request->result) {
      case -1:
        argv[0] = Exception::TypeError(String::New("Could not open source file"));
        break;
      case -2:
        argv[0] = Exception::TypeError(String::New("Premature End of JPEG file"));
        break;
      case -3:
        argv[0] = Exception::TypeError(String::New("Invalid Exif data"));
        break;
      default:
        argv[0] = Exception::TypeError(String::New("Unknown fatal error"));
    }
  }
  else {
    argv[0] = Undefined();
    argv[1] = Integer::New(request->result);    
  }

  TryCatch try_catch;

  request->callback->Call(Context::GetCurrent()->Global(), 2, argv);

  if (try_catch.HasCaught()) 
  {
      node::FatalException(try_catch);
  }

  request->callback.Dispose();

  free(request->filename);
  delete request;
}

char *get(v8::Local<v8::Value> value, const char *fallback = "") {
    if (value->IsString()) {
        v8::String::AsciiValue string(value);
        char *str = (char *) malloc(string.length() + 1);
        strcpy(str, *string);
        return str;
    }
    char *str = (char *) malloc(strlen(fallback) + 1);
    strcpy(str, fallback);
    return str;
}

static Handle<Value> JpegOrientation(const Arguments& args)
{
    HandleScope scope;

    ssize_t int1;
    char *filename;
    int callbackArg;
    if (args.Length() == 2 && args[0]->IsString() )
    {
      int1 = 0;
      callbackArg = 1;
      filename = get(args[0]);
    }
    else if ( args.Length() == 3 && args[0]->IsString() && args[1]->IsNumber() )
    {
      int1 = args[1]->Int32Value();
      callbackArg = 2;
      filename = get(args[0]);
    }
    else
    {
      return ThrowException(Exception::TypeError(String::New("Bad argument")));
    }

    if ( args[callbackArg]->IsFunction() )
    {
        Local<Function> callback = Local<Function>::Cast(args[callbackArg]);

        Orientation_req* request = new Orientation_req;
        request->callback = Persistent<Function>::New(callback);

        request->orientation = int1;
        request->filename = filename;

        uv_work_t* req = new uv_work_t();
        req->data = request;

        uv_queue_work(uv_default_loop(), req, OrientationWorker, (uv_after_work_cb) OrientationAfter);
    }
    else
    {
        return ThrowException(Exception::TypeError(String::New("Callback missing")));
    }

    return Undefined();
}


void init(Handle<Object> exports) {
  exports->Set(String::NewSymbol("orientation"), FunctionTemplate::New(JpegOrientation)->GetFunction());
}

NODE_MODULE(jpegexiforient, init)


//
// Code below this comment block is modified from:
//  http://jpegclub.org/exif_orientation.html
//

/* Read one byte, testing for EOF */
static int read_1_byte (FILE *myfile)
{
  int c;

  c = getc(myfile);
  if (c == EOF) return __INVALID_EXIF_DATA;

  return c;
}

/* Read 2 bytes, convert to unsigned int */
/* All 2-byte quantities in JPEG markers are MSB first */
static unsigned int read_2_bytes (FILE *myfile)
{
  int c1, c2;

  c1 = getc(myfile);
  if (c1 == EOF) return __EOF;
  c2 = getc(myfile);
  if (c2 == EOF) return __EOF;
  return (((unsigned int) c1) << 8) + ((unsigned int) c2);
}

/*
 * The main program.
 */

int get_set_orientation (int orientation, char *filename)
{
  unsigned char exif_data[65536L];

  FILE * myfile;
  int n_flag, set_flag;
  unsigned int length = 0, i = 0, tmp, exif_offset = 0; // silence warnings
  int is_motorola; /* Flag for byte order */
  unsigned int offset, number_of_tags, tagnum;

  n_flag = 0; set_flag = 0;

  set_flag = orientation;

  if (set_flag) {
    if ((myfile = fopen(filename, "rb+")) == NULL) {
      return __CANT_OPEN;
    }
  } else {
    if ((myfile = fopen(filename, "rb")) == NULL) {
      return __CANT_OPEN;
    }
  }

  /* Read File head, check for JPEG SOI + Exif APP1 */
  for (i = 0; i < 4; i++){
    tmp = read_1_byte(myfile);
    exif_data[i] = (unsigned char) tmp;
    if (tmp == __EOF) { fclose(myfile); return __PREMATURE_END; }
  }
  if (exif_data[0] != 0xFF ||
      exif_data[1] != 0xD8 ||
      exif_data[2] != 0xFF) { fclose(myfile); return __INVALID_EXIF_DATA; }

  if (exif_data[3] != 0xE1 &&
      exif_data[3] != 0xE0) { fclose(myfile); return __INVALID_EXIF_DATA; }

  // Skip JFIF data if it exists
  if (exif_data[3] == 0xE0) {
    length = read_2_bytes(myfile);
    if (length == __EOF) { fclose(myfile); return __PREMATURE_END; }
    fseek(myfile, length-1, SEEK_CUR);
    exif_offset = length+2;
    tmp = read_1_byte(myfile);
    exif_data[3] = (unsigned char) tmp;
    if (tmp == __EOF) { fclose(myfile); return __PREMATURE_END; }

    if (exif_data[3] != 0xE1) {
      fclose(myfile); 
      return __INVALID_EXIF_DATA;
    }
  }

  /* Get the marker parameter length count */
  length = read_2_bytes(myfile);
  if (length == __EOF) {
    fclose(myfile);
    return __PREMATURE_END;
  }
  /* Length includes itself, so must be at least 2 */
  /* Following Exif data length must be at least 6 */
  if (length < 8) { fclose(myfile); return __INVALID_EXIF_DATA; }
  length -= 8;

  /* Read Exif head, check for "Exif" */
  for (i = 0; i < 6; i++) {
    tmp = read_1_byte(myfile);
    exif_data[i] = (unsigned char) tmp;
    if (tmp == __EOF) { fclose(myfile); return __PREMATURE_END; }
  }
  if (exif_data[0] != 0x45 ||
      exif_data[1] != 0x78 ||
      exif_data[2] != 0x69 ||
      exif_data[3] != 0x66 ||
      exif_data[4] != 0 ||
      exif_data[5] != 0) {
    fclose(myfile); 
    return __INVALID_EXIF_DATA;
  }
  /* Read Exif body */
  for (i = 0; i < length; i++){
    tmp = read_1_byte(myfile);
    exif_data[i] = (unsigned char) tmp;
    if (tmp == __EOF) { fclose(myfile); return __PREMATURE_END; }
  }

  if (length < 12) { fclose(myfile); return __INVALID_EXIF_DATA; }/* Length of an IFD entry */

  /* Discover byte order */
  if (exif_data[0] == 0x49 && exif_data[1] == 0x49)
    is_motorola = 0;
  else if (exif_data[0] == 0x4D && exif_data[1] == 0x4D)
    is_motorola = 1;
  else { fclose(myfile); return __INVALID_EXIF_DATA; }

  /* Check Tag Mark */
  if (is_motorola) {
    if (exif_data[2] != 0) { fclose(myfile); return __INVALID_EXIF_DATA; }
    if (exif_data[3] != 0x2A) { fclose(myfile); return __INVALID_EXIF_DATA; }
  } else {
    if (exif_data[3] != 0) { fclose(myfile); return __INVALID_EXIF_DATA; }
    if (exif_data[2] != 0x2A) { fclose(myfile); return __INVALID_EXIF_DATA; }
  }

  /* Get first IFD offset (offset to IFD0) */
  if (is_motorola) {
    if (exif_data[4] != 0) { fclose(myfile); return __INVALID_EXIF_DATA; }
    if (exif_data[5] != 0) { fclose(myfile); return __INVALID_EXIF_DATA; }
    offset = exif_data[6];
    offset <<= 8;
    offset += exif_data[7];
  } else {
    if (exif_data[7] != 0) { fclose(myfile); return __INVALID_EXIF_DATA; }
    if (exif_data[6] != 0) { fclose(myfile); return __INVALID_EXIF_DATA; }
    offset = exif_data[5];
    offset <<= 8;
    offset += exif_data[4];
  }
  if (offset > length - 2) { fclose(myfile); return __INVALID_EXIF_DATA; } /* check end of data segment */

  /* Get the number of directory entries contained in this IFD */
  if (is_motorola) {
    number_of_tags = exif_data[offset];
    number_of_tags <<= 8;
    number_of_tags += exif_data[offset+1];
  } else {
    number_of_tags = exif_data[offset+1];
    number_of_tags <<= 8;
    number_of_tags += exif_data[offset];
  }
  if (number_of_tags == 0) { fclose(myfile); return __INVALID_EXIF_DATA; }
  offset += 2;

  /* Search for Orientation Tag in IFD0 */
  for (;;) {
    if (offset > length - 12) { fclose(myfile); return 0; } /* check end of data segment */
                // Tag not found... Return 0, its not invalid
    /* Get Tag number */
    if (is_motorola) {
      tagnum = exif_data[offset];
      tagnum <<= 8;
      tagnum += exif_data[offset+1];
    } else {
      tagnum = exif_data[offset+1];
      tagnum <<= 8;
      tagnum += exif_data[offset];
    }
    if (tagnum == 0x0112) break; /* found Orientation Tag */
    if (--number_of_tags == 0) { fclose(myfile); return 0; }// Tag not found... Return 0, its not invalid
    offset += 12;
  }
   
  if (set_flag) {
    /* Set the Orientation value */
    if (is_motorola) {
      exif_data[offset+2] = 0; /* Format = unsigned short (2 octets) */
      exif_data[offset+3] = 3;
      exif_data[offset+4] = 0; /* Number Of Components = 1 */
      exif_data[offset+5] = 0;
      exif_data[offset+6] = 0;
      exif_data[offset+7] = 1;
      exif_data[offset+8] = 0;
      exif_data[offset+9] = (unsigned char)set_flag;
      exif_data[offset+10] = 0;
      exif_data[offset+11] = 0;
    } else {
      exif_data[offset+2] = 3; /* Format = unsigned short (2 octets) */
      exif_data[offset+3] = 0;
      exif_data[offset+4] = 1; /* Number Of Components = 1 */
      exif_data[offset+5] = 0;
      exif_data[offset+6] = 0;
      exif_data[offset+7] = 0;
      exif_data[offset+8] = (unsigned char)set_flag;
      exif_data[offset+9] = 0;
      exif_data[offset+10] = 0;
      exif_data[offset+11] = 0;
    }
    fseek(myfile, (4 + 2 + 6 + 2) + offset + exif_offset, SEEK_SET);
    fwrite(exif_data + 2 + offset, 1, 10, myfile);
  } else {
    /* Get the Orientation value */
    if (is_motorola) {
      if (exif_data[offset+8] != 0) { fclose(myfile); return __INVALID_EXIF_DATA; }
      set_flag = exif_data[offset+9];
    } else {
      if (exif_data[offset+9] != 0) { fclose(myfile); return __INVALID_EXIF_DATA; }
      set_flag = exif_data[offset+8];
    }
    if (set_flag > 8) { fclose(myfile); return __INVALID_EXIF_DATA; }
  }
  fclose(myfile);
  return set_flag;
}
