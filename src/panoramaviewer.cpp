#include <QDebug>
#include <QApplication>
#include <QGraphicsProxyWidget>
#include <QToolTip>
#include <QPushButton>

#include "panoramaviewer.h"
#include "editview.h"

#define CHANNELS_COUNT 4

PanoramaViewer::PanoramaViewer(QWidget *parent) :
    QGraphicsView(parent)
{

    // Remove side bars on the panorama viewer
    this->verticalScrollBar()->blockSignals(true);
    this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->horizontalScrollBar()->blockSignals(true);
    this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    this->resizePoint = Point::None;

    // Initialize position container
    this->position.start_x = 0;
    this->position.start_y = 0;
    this->position.start_azimuth = 0.0;
    this->position.start_elevation = 0.0;
    this->position.azimuth = 0.0;
    this->position.elevation = 0.0;
    this->position.old_azimuth = 0.0;
    this->position.old_elevation = 0.0;
    this->position.old_width = 0;
    this->position.old_height = 0;

    // Initialize default mode
    this->mode = Mode::None;

    this->rect_list_index = 0;

    // Configure default settings
    this->scale_factor = 1.0;
    this->zoom_min = 20.0;
    this->zoom_max = 120.0;
    this->position.aperture_delta = 100.0;
    this->position.aperture = ( this->position.aperture_delta * ( LG_PI / 180.0 ) );;
    this->position.old_aperture = this->position.aperture;
    this->threads_count = 1;
    this->pixmap_initialized = false;

    this->increation_rect.rect = NULL;

    this->selected_rect = NULL;

    this->moveEnabled = true;
    this->zoomEnabled = true;
    this->createEnabled = true;
    this->editEnabled = true;

    // Create default scene
    this->scene = new QGraphicsScene();
    this->setScene(this->scene);

    QPen sight_pen;
    sight_pen.setColor( Qt::red );
    sight_pen.setWidth( 2 );

    this->sight_width = 800;
    this->sight = this->scene->addRect( this->dest_image_map.width() / 2, this->dest_image_map.height() / 2, this->sight_width, this->sight_width , sight_pen);

    this->pressed_keys.CTRL = false;

    // Connect signal for labels refresh
    connect(this, SIGNAL(refreshLabels()), parent, SLOT(refreshLabels()));

    connect(this, SIGNAL(updateScaleSlider(int)), parent, SLOT(updateScaleSlider(int)));


}

inline float clamp(float x, float a, float b)
{
    return x < a ? a : (x > b ? b : x);
}

float clampRad(float x, float a, float b)
{
    if(x > b)
    {
        float delta = (x - b);
        return (a + delta);
    } else if (x < a) {
        float delta = (a - x);
        return (b - delta);
    } else {
        return x;
    }
}

void PanoramaViewer::setup(int width, int height, float scale_factor, float zoom_min, float zoom_max, float zoom_def, int threads)
{
    this->scale_factor = scale_factor;
    this->zoom_min = zoom_min;
    this->zoom_max = zoom_max;
    this->position.aperture_delta = zoom_def;
    this->position.aperture = (zoom_def * (LG_PI / 180.0) );
    this->threads_count = threads;
    this->resize( width, height );
}

void PanoramaViewer::loadImage(QString path)
{
    this->image_path = path;
    this->src_image.load(path);
    this->src_image_map = QPixmap::fromImage(this->src_image);

    this->render();

}

void PanoramaViewer::loadImage(QImage image)
{
    this->src_image = image;
    this->src_image_map = QPixmap::fromImage(image);

    this->render();
}

void PanoramaViewer::updateScene(float azimuth, float elevation, float zoom)
{

    int WIDTH = this->width();
    int HEIGHT = this->height();

    int DEST_WIDTH = WIDTH * scale_factor;
    int DEST_HEIGHT = HEIGHT * scale_factor;

    float AZIMUTH = clampRad(azimuth, -360.0, 360.0);
    float ELEVATION = clamp(elevation, -90.0, 90.0);

    this->position.old_width = this->dest_image.width();
    this->position.old_height = this->dest_image.height();

    this->dest_image = QImage (DEST_WIDTH, DEST_HEIGHT, QImage::Format_RGB32);

    lg_etg_apperturep(

        ( inter_C8_t * ) this->src_image.bits(),
        this->src_image.width(),
        this->src_image.height(),
        CHANNELS_COUNT,
        ( inter_C8_t * ) dest_image.bits(),
        DEST_WIDTH,
        DEST_HEIGHT,
        CHANNELS_COUNT,
        AZIMUTH,
        ELEVATION,
        0.0,
        zoom,
        li_bilinearf,
        this->threads_count

    );

    this->dest_image_map = QPixmap::fromImage(this->dest_image);

    if(this->pixmap_initialized)
    {
        this->scene->removeItem(this->last_pixmap);
        delete this->last_pixmap;
    } else {
        this->pixmap_initialized = true;
    }

    this->last_pixmap = this->scene->addPixmap(this->dest_image_map);
    this->last_pixmap->setZValue(-1);

    this->scene->setSceneRect(this->dest_image_map.rect());
    this->fitInView(this->dest_image_map.rect());

    this->sight->setPos( QPointF( (DEST_WIDTH / 2) - ((this->sight_width / 2) * (this->scale_factor / this->position.aperture)),
                                  (DEST_HEIGHT / 2) - ((this->sight_width / 2) * (this->scale_factor / this->position.aperture)) ) );

    this->sight->setScale( this->scale_factor / this->position.aperture );
}

void PanoramaViewer::render()
{
    this->updateScene(
        this->position.azimuth,
        this->position.elevation,
        this->position.aperture
    );

    foreach(ObjectRect* rect, this->rect_list)
    {

        // Check that scene has not moved
        if(rect->getAutomaticStatus() == "None")
        {
            if( rect->proj_azimuth() != this->position.azimuth ||
                    rect->proj_elevation() != this->position.elevation ||
                    rect->proj_aperture() != this->position.aperture )
            {
                rect->setResizeEnabled( false );
            } else {
                rect->setResizeEnabled( true );
            }
        } else {
            rect->setResizeEnabled( false );
        }

        rect->mapTo(this->dest_image_map.width(),
                    this->dest_image_map.height(),
                    this->position.azimuth,
                    this->position.elevation,
                    this->position.aperture);

        if( this->isObjectVisible( rect ) )
        {
            rect->setVisible( true );
        } else {
            rect->setVisible( false );
        }
    }
}

// Function to update zoom of current scene
void PanoramaViewer::setZoom(float zoom_level)
{
    this->backupPosition();

    // Convert zoom value to radians
    this->position.aperture = ( zoom_level * ( LG_PI / 180.0 ) );
    this->position.aperture_delta = zoom_level;

    // Render scene
    this->render();
}

void PanoramaViewer::setView(float azimuth, float elevation)
{
    this->position.azimuth = azimuth;
    this->position.elevation = elevation;
    this->render();
}

// Mouse wheel handler
void PanoramaViewer::wheelEvent(QWheelEvent* event)
{
    // Determine delta
    int delta = (event->delta() / 120);

    if( this->pressed_keys.CTRL )
    {
        this->scale_factor = (this->scale_factor + (delta / 50.0));
        this->scale_factor = clamp(this->scale_factor, 0.1, 1.0);
        emit updateScaleSlider( this->scale_factor * 10 );
        this->render();
    } else {

        // Check if zoom is enabled
        if(!this->zoomEnabled)
            return;

        // Update current zoom level
        float old_zoom = this->position.aperture_delta;
        this->position.aperture_delta -= (delta * 1.5);
        this->position.aperture_delta = clamp(this->position.aperture_delta, this->zoom_min, this->zoom_max);

        // Apply zoom level
        if(this->position.aperture_delta != old_zoom)
            this->setZoom(this->position.aperture_delta);

    }
}

// Mouse buttons click handler
void PanoramaViewer::mousePressEvent(QMouseEvent* event)
{
    // Store normalized mouse coords
    QPointF mouse_scene = this->mapToScene(event->pos());

    // Check presence of left click
    if(event->buttons() & Qt::LeftButton)
    {
        // Exif function if move is disabled
        if(!this->moveEnabled)
            return;

        // Enable mouse tracking
        this->setMouseTracking(true);

        // Switch in mooving mode
        this->mode = Mode::Move;

        // Store base positions (Used to determine offset to move in panorama later)
        this->position.start_x = mouse_scene.x();
        this->position.start_y = mouse_scene.y();

        // Store base directions (Used to determine offset to move in panorama later)
        this->position.start_azimuth = this->position.azimuth / (LG_PI / 180.0);
        this->position.start_elevation = this->position.elevation / (LG_PI / 180.0);

    }
    // Check presence of right click
    else if (event->buttons() & Qt::RightButton)
    {
        QGraphicsPolygonItem* clicked_poly = qgraphicsitem_cast<QGraphicsPolygonItem*>(this->itemAt(event->x(), event->y()));

        if (clicked_poly != NULL)
        {
            ObjectRect* clicked_rect = qgraphicsitem_cast<ObjectRect*>(clicked_poly->parentItem());

            // Verify that clicked object is a widget and is not null
            if (clicked_rect != NULL)
            {
                // Convert object to ObjectRect
                this->selected_rect = clicked_rect;
            }
        }

        if(this->selected_rect)
        {

            qDebug() << "Mode::MoveCreate";
            // Store base positions (Used to determine offset to move in panorama later)
            this->position.start_x = mouse_scene.x();
            this->position.start_y = mouse_scene.y();

            // Determine mouse click pointer offset
            this->position.offset_1 = QPointF(
                                          mouse_scene.x() - this->selected_rect->getPoint1().x(),
                                          mouse_scene.y() - this->selected_rect->getPoint1().y()
                                      );

            // Determine mouse click pointer offset
            this->position.offset_2 = QPointF(
                                          mouse_scene.x() - this->selected_rect->getPoint2().x(),
                                          mouse_scene.y() - this->selected_rect->getPoint2().y()
                                      );

            // Determine mouse click pointer offset
            this->position.offset_3 = QPointF(
                                          mouse_scene.x() - this->selected_rect->getPoint3().x(),
                                          mouse_scene.y() - this->selected_rect->getPoint3().y()
                                      );

            // Determine mouse click pointer offset
            this->position.offset_4 = QPointF(
                                          mouse_scene.x() - this->selected_rect->getPoint4().x(),
                                          mouse_scene.y() - this->selected_rect->getPoint4().y()
                                      );

            if(!this->selected_rect->isResizeEnabled())
                return;

            if( (this->position.offset_3.x() > 0) && (this->position.offset_3.y() > 0) )
            {
                this->resizePoint = Point::Point3;
                this->mode = Mode::MoveResize;
            } else {
                // Switch in creation mode
                this->mode = Mode::MoveCreate;
            }

            // Enable mouse tracking
            this->setMouseTracking(true);

        } else {

            // Check if creation is enabled
            if(!this->createEnabled)
                return;

            // Check if mouse in in sight
            if(!this->isInSight( mouse_scene ))
                return;

            qDebug() << "Mode::Create";

            // Switch in creation mode
            this->mode = Mode::Create;

            // Enable mouse tracking
            this->setMouseTracking(true);

            // Store base positions (Used to determine size and position of object later)
            this->increation_rect.start_x = mouse_scene.x();
            this->increation_rect.start_y = mouse_scene.y();

        }
    }
}

void PanoramaViewer::mouseDoubleClickEvent(QMouseEvent *event)
{
    if(!this->editEnabled)
        return;

    QGraphicsPolygonItem* clicked_poly = qgraphicsitem_cast<QGraphicsPolygonItem*>(this->itemAt(event->x(), event->y()));

    if(clicked_poly != NULL)
    {
        ObjectRect* clicked_rect = qgraphicsitem_cast<ObjectRect*>(clicked_poly->parentItem());

        // Verify that clicked object is a widget and is not null
        if (clicked_rect != NULL)
        {
            if( (event->buttons() == Qt::RightButton) )
            {
                this->position.azimuth = clicked_rect->proj_azimuth();
                this->position.elevation = clicked_rect->proj_elevation();
                this->position.aperture = clicked_rect->proj_aperture();
                this->position.aperture_delta = (this->position.aperture / (LG_PI / 180.0));
                this->render();
            } else if ( event->buttons() == Qt::LeftButton )
            {
                EditView* w = new EditView(this, clicked_rect, NULL, EditMode::Scene);
                w->setAttribute( Qt::WA_DeleteOnClose );
                w->show();
            }
        }
    }
}

// Mouse buttons release handler
void PanoramaViewer::mouseReleaseEvent(QMouseEvent *)
{
    // Reset mode
    this->mode = Mode::None;

    // Reset temporary object
    if( this->increation_rect.rect )
    {
        this->increation_rect.rect->setProjectionPoints();
        this->increation_rect.rect = NULL;
    }
    this->selected_rect = NULL;

    // Disable mouse tracking
    this->setMouseTracking(false);
}

// Mouse displacement handler
void PanoramaViewer::mouseMoveEvent(QMouseEvent* event)
{
    // Store normalized mouse coords
    QPointF mouse_scene = this->mapToScene(event->pos());

    // Store mouse coords
    int MouseX = mouse_scene.x();
    int MouseY = mouse_scene.y();

    // Moving mouse section (move in panorama)
    if(this->mode == Mode::None)
    {

    }
    else if(this->mode == Mode::Move)
    {

        // Determine the displacement delta
        int delta_x = (MouseX - this->position.start_x);
        int delta_y = (MouseY - this->position.start_y);

        // Apply delta to azimuth and elevation
        float azimuth   = (this->position.start_azimuth   - ( (delta_x * this->position.aperture) * 0.1 ) / (this->scale_factor * 2.0) );
        float elevation = (this->position.start_elevation + ( (delta_y * this->position.aperture) * 0.1 ) / (this->scale_factor * 2.0) );

        this->backupPosition();

        // Clamp azimuth and elevation
        this->position.azimuth   = clampRad(azimuth, -360.0, 360.0) * (LG_PI / 180.0);
        this->position.elevation = clamp(elevation, -90.0, 90.0) * (LG_PI / 180.0);

        // Render scene
        this->render();

    }
    // Section creation section (create a new object)
    else if (this->mode == Mode::Create)
    {

        // Check if mouse in in sight
        if(!this->isInSight( mouse_scene ))
            return;

        // Check if temporary object is created
        if(this->increation_rect.rect == NULL)
        {
            this->increation_rect.rect = new ObjectRect();
            this->increation_rect.rect->setObjectRectType(ObjectRectType::Manual);
            this->increation_rect.rect->setType(ObjectType::None);
            this->increation_rect.rect->setObjectRectState(ObjectRectState::Valid);
            this->increation_rect.rect->setManualStatus("Valid");
            this->increation_rect.rect->setBlurred(true);

            this->increation_rect.rect->setId( this->rect_list_index++ );

            this->rect_list.append( this->increation_rect.rect );
            this->scene->addItem( this->increation_rect.rect );

            this->increation_rect.rect->setProjectionParametters(this->position.azimuth,
                    this->position.elevation,
                    this->position.aperture,
                    this->dest_image.width(),
                    this->dest_image.height());

            this->increation_rect.rect->setSourceImagePath( this->image_path );

            // Move selection object to mouse coords
            this->increation_rect.rect->setPoints(
                mouse_scene,
                mouse_scene,
                mouse_scene,
                mouse_scene
            );

            // Refresh main window labels
            emit refreshLabels();

        } else  {

            float mouse_x = clamp(mouse_scene.x(), this->increation_rect.rect->getPoint1().x() + 4.0, this->width());
            float mouse_y = clamp(mouse_scene.y(), this->increation_rect.rect->getPoint1().y() + 4.0, this->height());

            // Move selection object to mouse coords
            this->increation_rect.rect->setPoint3_Rigid(
                QPointF(mouse_x, mouse_y)
            );

            QToolTip::showText(event->globalPos(),
                               QString::number( (int) (this->increation_rect.rect->getSize().width() / this->scale_factor) ) + "x" +
                               QString::number( (int) (this->increation_rect.rect->getSize().height() / this->scale_factor )),
                               this, rect() );

        }
    } else if(this->mode == Mode::MoveCreate)
    {
        // Check if mouse and rect are in in sight
        QVector<QPointF> points = this->selected_rect->simulate_moveObject(mouse_scene,
                                  this->position.offset_1,
                                  this->position.offset_2,
                                  this->position.offset_3,
                                  this->position.offset_4);

        if(!this->isObjectInSight( points[0], points[1], points[2], points[3] ))
            return;

        qDebug() << "Mode::MoveCreate";

        // Check that scene has not moved
        if( this->selected_rect->proj_azimuth() != this->position.azimuth ||
                this->selected_rect->proj_elevation() != this->position.elevation ||
                this->selected_rect->proj_aperture() != this->position.aperture)
        {
            return;
        }

        this->selected_rect->moveObject(mouse_scene,
                                        this->position.offset_1,
                                        this->position.offset_2,
                                        this->position.offset_3,
                                        this->position.offset_4);

        this->selected_rect->setProjectionParametters(this->position.azimuth,
                this->position.elevation,
                this->position.aperture,
                this->dest_image.width(),
                this->dest_image.height());

        this->selected_rect->setProjectionPoints();
    } else if(this->mode == Mode::MoveResize)
    {
        // Check if mouse and rect are in in sight
        if(!this->isObjectInSight( this->selected_rect->getPoint1(),
                                   this->selected_rect->getPoint2(),
                                   this->selected_rect->getPoint3(),
                                   this->selected_rect->getPoint4()) && !this->isInSight( mouse_scene ))
            return;

        // Check that scene has not moved
        if( this->selected_rect->proj_azimuth() != this->position.azimuth ||
                this->selected_rect->proj_elevation() != this->position.elevation ||
                this->selected_rect->proj_aperture() != this->position.aperture)
        {
            return;
        }

        float mouse_x = clamp(mouse_scene.x(), this->selected_rect->getPoint1().x() + 4.0, this->width());
        float mouse_y = clamp(mouse_scene.y(), this->selected_rect->getPoint1().y() + 4.0, this->height());

        // Move selection object to mouse coords
        switch(this->resizePoint)
        {
        case Point::Point3:
            this->selected_rect->setPoint3_Rigid(
                QPointF(mouse_x, mouse_y),
                this->position.offset_3
            );
            break;

        }

        this->selected_rect->setProjectionParametters(this->position.azimuth,
                this->position.elevation,
                this->position.aperture,
                this->dest_image.width(),
                this->dest_image.height());

        this->selected_rect->setProjectionPoints();

        QToolTip::showText(event->globalPos(),
                           QString::number( (int) (this->selected_rect->getSize().width() / this->scale_factor) ) + "x" +
                           QString::number( (int) (this->selected_rect->getSize().height() / this->scale_factor )),
                           this, rect() );
    }
}

// Viewer resize handler
void PanoramaViewer::resizeEvent(QResizeEvent *)
{

    // Store actual dimensions
    int WIDTH = this->width();
    int HEIGHT = this->height();

    // Check if dimensions has changed
    if(WIDTH != this->previous_width && HEIGHT != this->previous_height)
    {

        // Store new dimensions
        this->previous_width = WIDTH;
        this->previous_height = HEIGHT;

        // Render scene
        this->render();
    }

}

// Function to crop a selection object
QImage PanoramaViewer::cropObject(ObjectRect* rect)
{

    // Convert points to a QRect and deduce borders sizes

    ObjectRect* rect_mapped = rect->copy();

    rect_mapped->mapTo(this->width(),
                       this->height(),
                       rect->proj_azimuth(),
                       rect->proj_elevation(),
                       rect->proj_aperture());
    rect_mapped->setProjectionPoints();

    QRect rect_sel(
        QPoint(rect_mapped->proj_point_1().x() + rect_mapped->getBorderWidth(), rect_mapped->proj_point_1().y() + rect_mapped->getBorderWidth()),
        QPoint(rect_mapped->proj_point_3().x() - rect_mapped->getBorderWidth(), rect_mapped->proj_point_3().y() - rect_mapped->getBorderWidth())
    );

    delete rect_mapped;

    QImage temp_dest(this->width(), this->height(), QImage::Format_RGB32);

    lg_etg_apperturep(

        ( inter_C8_t * ) this->src_image.bits(),
        this->src_image.width(),
        this->src_image.height(),
        CHANNELS_COUNT,
        ( inter_C8_t * ) temp_dest.bits(),
        this->width(),
        this->height(),
        CHANNELS_COUNT,
        rect->proj_azimuth(),
        rect->proj_elevation(),
        0.0,
        rect->proj_aperture(),
        li_bilinearf,
        this->threads_count
    );

    // Crop and return image
    return temp_dest.copy(rect_sel);

}

void PanoramaViewer::backupPosition()
{
    this->position.old_azimuth = this->position.azimuth;
    this->position.old_elevation = this->position.elevation;
    this->position.old_aperture = this->position.aperture;
}

bool PanoramaViewer::isObjectVisible(ObjectRect *rect)
{

    int state = g2g_point(rect->proj_width(),
                          rect->proj_height(),
                          rect->proj_azimuth(),
                          rect->proj_elevation(),
                          rect->proj_aperture(),
                          rect->proj_point_1().x(),
                          rect->proj_point_1().y(),

                          this->dest_image_map.width(),
                          this->dest_image_map.height(),
                          this->position.azimuth,
                          this->position.elevation,
                          this->position.aperture);

    int state2 = g2g_point(rect->proj_width(),
                           rect->proj_height(),
                           rect->proj_azimuth(),
                           rect->proj_elevation(),
                           rect->proj_aperture(),
                           rect->proj_point_2().x(),
                           rect->proj_point_2().y(),

                           this->dest_image_map.width(),
                           this->dest_image_map.height(),
                           this->position.azimuth,
                           this->position.elevation,
                           this->position.aperture);

    int state3 = g2g_point(rect->proj_width(),
                           rect->proj_height(),
                           rect->proj_azimuth(),
                           rect->proj_elevation(),
                           rect->proj_aperture(),
                           rect->proj_point_3().x(),
                           rect->proj_point_3().y(),

                           this->dest_image_map.width(),
                           this->dest_image_map.height(),
                           this->position.azimuth,
                           this->position.elevation,
                           this->position.aperture);

    int state4 = g2g_point(rect->proj_width(),
                           rect->proj_height(),
                           rect->proj_azimuth(),
                           rect->proj_elevation(),
                           rect->proj_aperture(),
                           rect->proj_point_4().x(),
                           rect->proj_point_4().y(),

                           this->dest_image_map.width(),
                           this->dest_image_map.height(),
                           this->position.azimuth,
                           this->position.elevation,
                           this->position.aperture);

    if( state != 1 || state2 != 1 || state3 != 1 || state4 != 1) {
        return false;
    } else {
        return true;
    }
}

bool PanoramaViewer::isInSight(QPointF pos, float tolerance)
{
    QSizeF sight_rect_size = (this->sight->boundingRect().size() * this->sight->scale());

    // Check if rect is in creation zone
    if (!((pos.x() + tolerance) >= this->sight->pos().x()
            &&(pos.x() + tolerance) < (this->sight->pos().x() + sight_rect_size.width())
            && (pos.y() + tolerance) >= this->sight->pos().y()
            && (pos.y() + tolerance) < (this->sight->pos().y() + sight_rect_size.height())))
    {
        return false;
    } else {
        return true;
    }
}

bool PanoramaViewer::isObjectInSight(QPointF p1, QPointF p2, QPointF p3, QPointF p4)
{
    if(!(this->isInSight( p1 )
            && this->isInSight( p2 )
            && this->isInSight( p3 )
            && this->isInSight( p4 )))
    {
        return false;
    } else {
        return true;
    }
}

void PanoramaViewer::setMoveEnabled(bool value)
{
    this->moveEnabled = value;
}

void PanoramaViewer::setZoomEnabled(bool value)
{
    this->zoomEnabled = value;
}

void PanoramaViewer::setCreateEnabled(bool value)
{
    this->createEnabled = value;
}

void PanoramaViewer::setEditEnabled(bool value)
{
    this->editEnabled = value;
}

void PanoramaViewer::refreshLabels_slot()
{
    emit refreshLabels();
}

void PanoramaViewer::updateScaleSlider_slot(int value)
{
    emit updateScaleSlider(value);
}


void PanoramaViewer::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Control)
        this->pressed_keys.CTRL = true;
}

void PanoramaViewer::keyReleaseEvent(QKeyEvent *)
{
    this->pressed_keys.CTRL = false;
}
