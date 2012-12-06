/*
   This file is part of ffmpeg-php

   Copyright (C) 2004-2008 Todd Kirby (ffmpeg.php AT gmail.com)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

   In addition, as a special exception, the copyright holders of ffmpeg-php 
   give you permission to combine ffmpeg-php with code included in the 
   standard release of PHP under the PHP license (or modified versions of 
   such code, with unchanged license). You may copy and distribute such a 
   system following the terms of the GNU GPL for ffmpeg-php and the licenses 
   of the other code concerned, provided that you include the source code of 
   that other code when and as the GNU GPL requires distribution of source code.

   You must obey the GNU General Public License in all respects for all of the 
   code used other than standard release of PHP. If you modify this file, you 
   may extend this exception to your version of the file, but you are not 
   obligated to do so. If you do not wish to do so, delete this exception 
   statement from your version.

 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"

#include <avcodec.h>
#include <avformat.h>

#if HAVE_SWSCALER
#include <swscale.h>
#endif

#include "php_ini.h"
#include "php_globals.h"
#include "ext/standard/info.h"

#include "php_ffmpeg.h"
#include "ffmpeg_errorhandler.h"

#define FFMPEG_PHP_VERSION "0.7.1"

zend_module_entry ffmpeg_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    "ffmpeg",
    NULL,
    PHP_MINIT(ffmpeg),
    PHP_MSHUTDOWN(ffmpeg),
    NULL,
    NULL,
    PHP_MINFO(ffmpeg),
#if ZEND_MODULE_API_NO >= 20010901
    FFMPEG_PHP_VERSION, 
#endif
    STANDARD_MODULE_PROPERTIES
};


#ifdef COMPILE_DL_FFMPEG
ZEND_GET_MODULE(ffmpeg);
#endif

#define FFMPEG_AVMEDIA_TYPE(flag) (flag == AVMEDIA_TYPE_VIDEO ? "v" : (flag == AVMEDIA_TYPE_AUDIO ? "a" : (flag == AVMEDIA_TYPE_SUBTITLE ? "s" : "u")))

extern void register_ffmpeg_movie_class(int);
extern void register_ffmpeg_frame_class(int);

PHP_INI_BEGIN()
    PHP_INI_ENTRY("ffmpeg.allow_persistent", "0", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("ffmpeg.show_warnings", "0", PHP_INI_ALL, NULL)
PHP_INI_END()


/* {{{ php module init function
 */
PHP_MINIT_FUNCTION(ffmpeg)
{
    /* register all codecs */
    av_register_all();
    
    REGISTER_INI_ENTRIES();
 
    if (INI_BOOL("ffmpeg.show_warnings")) {
        av_log_set_callback(ffmpeg_errorhandler);
    } else {
        av_log_set_callback(ffmpeg_hide_errors);
    }
   
    register_ffmpeg_movie_class(module_number);
    register_ffmpeg_frame_class(module_number);

    REGISTER_STRING_CONSTANT("FFMPEG_PHP_VERSION_STRING", 
		    FFMPEG_PHP_VERSION, CONST_CS | CONST_PERSISTENT);
    REGISTER_STRING_CONSTANT("FFMPEG_PHP_BUILD_DATE_STRING", 
		    __DATE__ " " __TIME__, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("LIBAVCODEC_VERSION_NUMBER", 
		    avcodec_version(), CONST_CS | CONST_PERSISTENT);
#ifdef LIBAVCODEC_BUILD
    REGISTER_LONG_CONSTANT("LIBAVCODEC_BUILD_NUMBER", 
		    LIBAVCODEC_BUILD, CONST_CS | CONST_PERSISTENT);
#else
    REGISTER_LONG_CONSTANT("LIBAVCODEC_BUILD_NUMBER",
            avcodec_build(), CONST_CS | CONST_PERSISTENT);
#endif

#if HAVE_LIBGD20
    REGISTER_LONG_CONSTANT("FFMPEG_PHP_GD_ENABLED", 1, CONST_CS | CONST_PERSISTENT);
#else
    REGISTER_LONG_CONSTANT("FFMPEG_PHP_GD_ENABLED", 0, CONST_CS | CONST_PERSISTENT);
#endif // HAVE_LIBGD20
 
    return SUCCESS;
}
/* }}} */


/* {{{ php module shutdown function
 */
PHP_MSHUTDOWN_FUNCTION(ffmpeg)
{
    // TODO: Free any remaining persistent movies here?
    
    UNREGISTER_INI_ENTRIES();

    return SUCCESS;
}
/* }}} */


/* {{{ php info function
   Add an entry for ffmpeg-php support in phpinfo() */
PHP_MINFO_FUNCTION(ffmpeg)
{
    php_info_print_table_start();
//    php_info_print_table_header(2, "ffmpeg-php", "enabled");
    php_info_print_table_row(2, "ffmpeg-php version", FFMPEG_PHP_VERSION);
    php_info_print_table_row(2, "ffmpeg-php built on", __DATE__ " "  __TIME__);
#if HAVE_LIBGD20
    php_info_print_table_row(2, "ffmpeg-php gd support ", "enabled");
#else
    php_info_print_table_row(2, "ffmpeg-php gd support ", "disabled");
#endif // HAVE_LIBGD20
    php_info_print_table_row(2, "ffmpeg libavcodec version", LIBAVCODEC_IDENT);
    php_info_print_table_row(2, "ffmpeg libavcodec license", avcodec_license()); //people need to know if they can distribute
    php_info_print_table_row(2, "ffmpeg libavformat version", LIBAVFORMAT_IDENT);
    php_info_print_table_row(2, "ffmpeg libavformat license", avformat_license());
#if HAVE_SWSCALER
    php_info_print_table_row(2, "ffmpeg swscaler version", LIBSWSCALE_IDENT);
    php_info_print_table_row(2, "ffmpeg swscaler license", swscale_license());
#else 
    php_info_print_table_row(2, "ffmpeg swscaler", "disabled");
#endif

    //phpinfo should show the codec list available to aid developers
    AVCodec *next_codec = NULL;
    char *m_codec_list = NULL;
    long m_codec_list_len = 0;
    long m_codec_len = 0;
    while((next_codec = av_codec_next(next_codec))) {
      //go through each codec and add to the list
      m_codec_len = (strlen(next_codec->name) + 5);
      m_codec_list_len += m_codec_len;
      m_codec_list = realloc(m_codec_list, m_codec_list_len);

      //wtf? always gives buffer overflows... 
      //sprintf(m_codec_list+(m_codec_list_len-m_codec_len), "%s(%c), ", next_codec->name, FFMPEG_AVMEDIA_TYPE(next_codec->type));

      //doing it the long way instead
      memcpy(m_codec_list+(m_codec_list_len-m_codec_len), next_codec->name, m_codec_len);
      memcpy(m_codec_list+(m_codec_list_len-5), "(", 1);
      memcpy(m_codec_list+(m_codec_list_len-4), FFMPEG_AVMEDIA_TYPE(next_codec->type), 1);
      memcpy(m_codec_list+(m_codec_list_len-3), ")", 1);
      memcpy(m_codec_list+(m_codec_list_len-2), ", ", 2);
    }

    m_codec_list = realloc(m_codec_list, m_codec_list_len+1);
    m_codec_list[m_codec_list_len] = (char)NULL;

    //give the user a list of available codecs
    //should really add (dec/enc) on the end of each to show each is capable of
    //consider giving each codec its own row in in a codec table displaying if the codec is cable of a/v/s and enc/dec
    php_info_print_table_row(2, "ffmpeg codec_list", m_codec_list);

    php_info_print_table_end();

    DISPLAY_INI_ENTRIES();
}
/* }}} */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4
 * vim<600: noet sw=4 ts=4
 */
